#define _POSIX_C_SOURCE 200809L
#include "http.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>

static int send_all(int fd, const char *buf, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, buf + sent, len - sent);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static void send_error(int fd, int code, const char *reason,
                       const char *message, int head_only)
{
    char body[512], header[1024];
    int body_len = snprintf(body, sizeof(body),
        "<!doctype html><html><body><h1>%d %s</h1><p>%s</p></body></html>\n",
        code, reason, message);
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.0 %d %s\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n\r\n",
        code, reason, body_len);
    if (header_len > 0) send_all(fd, header, (size_t)header_len);
    if (!head_only && body_len > 0) send_all(fd, body, (size_t)body_len);
}

static const char *content_type(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (!strcasecmp(ext, ".html") || !strcasecmp(ext, ".htm"))
        return "text/html; charset=utf-8";
    if (!strcasecmp(ext, ".txt")) return "text/plain; charset=utf-8";
    if (!strcasecmp(ext, ".css")) return "text/css";
    if (!strcasecmp(ext, ".js")) return "application/javascript";
    if (!strcasecmp(ext, ".jpg") || !strcasecmp(ext, ".jpeg"))
        return "image/jpeg";
    if (!strcasecmp(ext, ".png")) return "image/png";
    if (!strcasecmp(ext, ".gif")) return "image/gif";
    if (!strcasecmp(ext, ".pdf")) return "application/pdf";
    if (!strcasecmp(ext, ".svg")) return "image/svg+xml";
    return "application/octet-stream";
}

void handle_http_request(int client_fd, const char *root, int verbose)
{
    char request[8192], method[16], uri[4096], version[32];
    size_t used = 0;

    while (used < sizeof(request) - 1) {
        ssize_t n = read(client_fd, request + used, sizeof(request) - 1 - used);
        if (n < 0) {
            if (errno == EINTR) continue;
            return;
        }
        if (n == 0) break;
        used += (size_t)n;
        request[used] = '\0';
        if (strstr(request, "\r\n\r\n") || strstr(request, "\n\n")) break;
    }

    if (used == 0) return;
    request[used] = '\0';

    if (sscanf(request, "%15s %4095s %31s", method, uri, version) != 3) {
        send_error(client_fd, 400, "Bad Request", "Invalid request line.", 0);
        return;
    }

    int head_only = !strcmp(method, "HEAD");
    if (strcmp(method, "GET") && !head_only) {
        send_error(client_fd, 501, "Not Implemented",
                   "Only GET and HEAD are supported.", 0);
        return;
    }

    if (strncmp(version, "HTTP/1.", 7) != 0) {
        send_error(client_fd, 400, "Bad Request", "Unsupported HTTP version.", head_only);
        return;
    }

    if (uri[0] != '/' || strstr(uri, "://")) {
        send_error(client_fd, 400, "Bad Request", "Only absolute-path URIs are supported.", head_only);
        return;
    }

    /* Remove query string; percent-encoded paths are not supported. */
    char *query = strchr(uri, '?');
    if (query) *query = '\0';
    if (strchr(uri, '%')) {
        send_error(client_fd, 400, "Bad Request", "Percent-encoded URIs are not supported.", head_only);
        return;
    }

    /* Prevent directory traversal. */
    if (strstr(uri, "/../") || !strcmp(uri, "/..") ||
        strstr(uri, "/./") || !strcmp(uri, "/.")) {
        send_error(client_fd, 403, "Forbidden", "Path traversal is not allowed.", head_only);
        return;
    }

    char relative[4096];
    if (!strcmp(uri, "/")) {
        strcpy(relative, "/index.html");
    } else {
        if (strlen(uri) >= sizeof(relative)) {
            send_error(client_fd, 414, "URI Too Long", "The URI is too long.", head_only);
            return;
        }
        strcpy(relative, uri);
    }

    char path[PATH_MAX];
    int pn = snprintf(path, sizeof(path), "%s%s", root, relative);
    if (pn < 0 || (size_t)pn >= sizeof(path)) {
        send_error(client_fd, 414, "URI Too Long", "The path is too long.", head_only);
        return;
    }

    struct stat st;
    if (stat(path, &st) < 0) {
        send_error(client_fd, 404, "Not Found", "The requested file was not found.", head_only);
        return;
    }
    if (!S_ISREG(st.st_mode)) {
        send_error(client_fd, 403, "Forbidden", "The requested resource is not a regular file.", head_only);
        return;
    }

    int file_fd = open(path, O_RDONLY);
    if (file_fd < 0) {
        send_error(client_fd, 403, "Forbidden", "The requested file cannot be opened.", head_only);
        return;
    }

    char header[1024];
    int hn = snprintf(header, sizeof(header),
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n\r\n",
        content_type(path), (long long)st.st_size);

    if (verbose) fprintf(stderr, "%s %s -> 200\n", method, uri);
    if (hn > 0) send_all(client_fd, header, (size_t)hn);

    if (!head_only) {
        char buffer[8192];
        ssize_t n;
        while ((n = read(file_fd, buffer, sizeof(buffer))) > 0) {
            if (send_all(client_fd, buffer, (size_t)n) < 0) break;
        }
    }
    close(file_fd);
}
