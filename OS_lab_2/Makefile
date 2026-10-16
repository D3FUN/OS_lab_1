CC = cc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic

.PHONY: all clean

all: main

main: main.c
	$(CC) $(CFLAGS) main.c -o main -pthread

clean:
	rm -f main
