.RECIPEPREFIX = >
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS = -Isrc
TARGET = sws
OBJECTS = src/sws.o src/http.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
>$(CC) $(CFLAGS) -o $@ $(OBJECTS)

src/%.o: src/%.c src/http.h
>$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
>rm -f $(OBJECTS) $(TARGET)
