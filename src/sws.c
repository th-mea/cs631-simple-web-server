#define _POSIX_C_SOURCE 200809L
#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static void usage(const char *name)
{
    fprintf(stderr, "Usage: %s [-f] [-v] [-p port] [-r document-root]\n", name);
    fprintf(stderr, "  -f  run in foreground\n");
    fprintf(stderr, "  -v  log requests (also runs in foreground)\n");
    fprintf(stderr, "  -p  listening port (default 8080)\n");
    fprintf(stderr, "  -r  document root (default ./www)\n");
}

static void reap_children(int sig)
{
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

static int make_daemon(void)
{
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) exit(EXIT_SUCCESS);
    if (setsid() < 0) return -1;

    pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) exit(EXIT_SUCCESS);

    if (chdir("/") < 0) return -1;
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    return 0;
}

int main(int argc, char **argv)
{
    int port = 8080, foreground = 0, verbose = 0, opt;
    char root[PATH_MAX] = "www";

    while ((opt = getopt(argc, argv, "fvp:r:h")) != -1) {
        switch (opt) {
        case 'f': foreground = 1; break;
        case 'v': verbose = 1; foreground = 1; break;
        case 'p': {
            char *end = NULL;
            long value = strtol(optarg, &end, 10);
            if (!end || *end || value < 1 || value > 65535) {
                fprintf(stderr, "Invalid port: %s\n", optarg);
                return EXIT_FAILURE;
            }
            port = (int)value;
            break;
        }
        case 'r':
            if (strlen(optarg) >= sizeof(root)) {
                fprintf(stderr, "Document root path is too long.\n");
                return EXIT_FAILURE;
            }
            strcpy(root, optarg);
            break;
        case 'h': usage(argv[0]); return EXIT_SUCCESS;
        default: usage(argv[0]); return EXIT_FAILURE;
        }
    }

    char absolute_root[PATH_MAX];
    if (!realpath(root, absolute_root)) {
        perror("document root");
        return EXIT_FAILURE;
    }
    strcpy(root, absolute_root);

    if (!foreground && make_daemon() < 0) {
        perror("daemon");
        return EXIT_FAILURE;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = reap_children;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        if (foreground) perror("socket");
        return EXIT_FAILURE;
    }

    int reuse = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((unsigned short)port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        if (foreground) perror("bind");
        close(server_fd);
        return EXIT_FAILURE;
    }
    if (listen(server_fd, 64) < 0) {
        if (foreground) perror("listen");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (verbose)
        fprintf(stderr, "Server listening on port %d; root=%s\n", port, root);

    for (;;) {
        struct sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_address, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            if (verbose) perror("accept");
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            close(server_fd);
            handle_http_request(client_fd, root, verbose);
            close(client_fd);
            _exit(EXIT_SUCCESS);
        }
        if (pid < 0 && verbose) perror("fork");
        close(client_fd);
    }
}
