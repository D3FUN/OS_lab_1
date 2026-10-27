CC = cc
CPPFLAGS = -D_POSIX_C_SOURCE=200809L
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
LDLIBS = -pthread

ifeq ($(shell uname -s),Linux)
LDLIBS += -lrt
endif

.PHONY: all test sanitize clean
all: parent child

parent: parent.c ipc.c ipc.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) parent.c ipc.c -o $@ $(LDLIBS)

child: child.c ipc.c ipc.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) child.c ipc.c -o $@ $(LDLIBS)

test: all
	python3 tests/test_lab3.py

sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS='-std=c11 -Wall -Wextra -Wpedantic -Werror -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' test

clean:
	rm -f parent child
