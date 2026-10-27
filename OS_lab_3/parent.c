#define _POSIX_C_SOURCE 200809L
#include "ipc.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define FILE_NAME_SIZE 4096

static volatile sig_atomic_t stop_requested = 0;

static void request_stop(int signal_number) {
    stop_requested = signal_number;
}

static ssize_t read_input(void *buffer, size_t length) {
    for (;;) {
        if (stop_requested) {
            errno = EINTR;
            return -1;
        }
        /* A timeout bounds the signal-before-wait race even on an idle stdin. */
        struct pollfd input = {STDIN_FILENO, POLLIN, 0};
        int ready = poll(&input, 1, 100);
        if (ready == -1 && errno == EINTR) continue;
        if (ready == -1) return -1;
        if (ready == 0) continue;
        if (stop_requested) continue;
        ssize_t count = read(STDIN_FILENO, buffer, length);
        if (count == -1 && errno == EINTR) continue;
        return count;
    }
}

static int read_file_name(char *name) {
    size_t length = 0;
    unsigned char character;
    for (;;) {
        ssize_t count = read_input(&character, 1);
        if (count == -1) return -1;
        if (count == 0 || character == '\n') break;
        if (character == '\0') {
            errno = EINVAL;
            return -1;
        }
        if (length == FILE_NAME_SIZE - 1) {
            errno = ENAMETOOLONG;
            return -1;
        }
        name[length++] = (char)character;
    }
    name[length] = '\0';
    if (length == 0) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

/* A bounded sleep also lets us detect a consumer that died without posting.
 * sem_timedwait is not available on macOS; the child uses blocking sem_wait. */
static int wait_empty(struct channel *channel, pid_t *child) {
    for (;;) {
        if (stop_requested) {
            errno = EINTR;
            return -1;
        }
        int status;
        pid_t ended = waitpid(*child, &status, WNOHANG);
        if (ended == *child) {
            *child = -1;
            errno = EPIPE;
            return -1;
        }
        if (ended == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (sem_trywait(channel->empty) == 0) return 0;
        if (errno == EINTR) continue;
        if (errno != EAGAIN) return -1;
        struct timespec pause = {0, 1000000L};
        if (nanosleep(&pause, NULL) == -1 && errno != EINTR) return -1;
    }
}

static int send_block(struct channel *channel, pid_t *child,
                      const unsigned char *data, size_t length, int end) {
    if (wait_empty(channel, child) == -1) return -1;
    if (length != 0) memcpy(channel->block->data, data, length);
    channel->block->length = length;
    channel->block->end_of_input = end;
    return sem_post(channel->ready);
}

static pid_t start_child(struct channel *channel, const int output[2], int index) {
    pid_t child = fork();
    if (child != 0) return child;
    struct sigaction action = {0};
    action.sa_handler = SIG_DFL;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) == -1 ||
        sigaction(SIGTERM, &action, NULL) == -1 || stop_requested) _exit(1);
    if (dup2(output[index], STDOUT_FILENO) == -1) {
        perror("child: dup2");
        _exit(1);
    }
    for (int i = 0; i < 2; ++i) {
        if (output[i] != STDOUT_FILENO) close(output[i]);
    }
    execl("./child", "child", channel->memory_name, channel->empty_name,
          channel->ready_name, (char *)NULL);
    perror("child: execl ./child");
    _exit(1);
}

int main(void) {
    struct channel channels[2];
    pid_t children[2] = {-1, -1};
    int output[2] = {-1, -1};
    int result = EXIT_FAILURE;
    for (int i = 0; i < 2; ++i) channel_init(&channels[i]);
    struct sigaction action = {0};
    action.sa_handler = request_stop;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) == -1 ||
        sigaction(SIGTERM, &action, NULL) == -1) {
        perror("sigaction");
        goto cleanup;
    }
    char names[2][FILE_NAME_SIZE];
    const char *prompts[2] = {"First file name: ", "Second file name: "};
    for (int i = 0; i < 2; ++i) {
        if (write_all(STDERR_FILENO, prompts[i], strlen(prompts[i])) == -1 ||
            read_file_name(names[i]) == -1) {
            perror("read file name");
            goto cleanup;
        }
    }
    struct stat info[2];
    for (int i = 0; i < 2; ++i) {
        output[i] = open(names[i], O_WRONLY | O_CREAT | O_NONBLOCK, 0644);
        if (output[i] == -1 || fstat(output[i], &info[i]) == -1) {
            perror("open output");
            goto cleanup;
        }
        if (!S_ISREG(info[i].st_mode)) {
            fprintf(stderr, "Output must be a regular file\n");
            goto cleanup;
        }
    }
    if (info[0].st_dev == info[1].st_dev && info[0].st_ino == info[1].st_ino) {
        fprintf(stderr, "Output files must be different (including hard links)\n");
        goto cleanup;
    }
    for (int i = 0; i < 2; ++i) {
        if (ftruncate(output[i], 0) == -1 ||
            channel_create(&channels[i], (unsigned int)i) == -1) {
            perror("prepare output / IPC");
            goto cleanup;
        }
    }
    for (int i = 0; i < 2; ++i) {
        children[i] = start_child(&channels[i], output, i);
        if (children[i] == -1) {
            perror("fork");
            goto cleanup;
        }
    }
    for (int i = 0; i < 2; ++i) {
        int fd = output[i];
        output[i] = -1;
        if (close(fd) == -1) {
            perror("close output in parent");
            goto cleanup;
        }
    }
    unsigned char input[BUFFER_SIZE], fragment[BUFFER_SIZE];
    size_t used = 0;
    int destination = 0;
    for (;;) {
        ssize_t count = read_input(input, sizeof(input));
        if (count == -1) {
            perror("read input");
            goto cleanup;
        }
        if (count == 0) break;
        for (ssize_t i = 0; i < count; ++i) {
            fragment[used++] = input[i];
            if (used == BUFFER_SIZE || input[i] == '\n') {
                if (send_block(&channels[destination], &children[destination],
                               fragment, used, 0) == -1) {
                    perror("send data (consumer may have exited)");
                    goto cleanup;
                }
                used = 0;
            }
            if (input[i] == '\n') destination = 1 - destination;
        }
    }
    if (used != 0 && send_block(&channels[destination], &children[destination],
                                fragment, used, 0) == -1) {
        perror("send final fragment");
        goto cleanup;
    }
    for (int i = 0; i < 2; ++i) {
        if (send_block(&channels[i], &children[i], NULL, 0, 1) == -1) {
            perror("send EOF");
            goto cleanup;
        }
    }
    result = EXIT_SUCCESS;
    for (int i = 0; i < 2; ++i) {
        int status;
        pid_t ended;
        for (;;) {
            if (stop_requested) {
                errno = EINTR;
                ended = -1;
                break;
            }
            ended = waitpid(children[i], &status, WNOHANG);
            if (ended > 0 || (ended == -1 && errno != EINTR)) break;
            struct timespec pause = {0, 1000000L};
            if (nanosleep(&pause, NULL) == -1 && errno != EINTR) {
                ended = -1;
                break;
            }
        }
        if (ended == -1) {
            perror("waitpid");
            result = EXIT_FAILURE;
            goto cleanup;
        }
        children[i] = -1;
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Child %d failed\n", i + 1);
            result = EXIT_FAILURE;
        }
    }
cleanup:
    for (int i = 0; i < 2; ++i) {
        if (children[i] > 0) {
            /* A stopped or broken consumer must not make error cleanup hang. */
            if (kill(children[i], SIGKILL) == -1 && errno != ESRCH) perror("kill");
            while (waitpid(children[i], NULL, 0) == -1 && errno == EINTR) {}
        }
        if (output[i] != -1 && close(output[i]) == -1) result = EXIT_FAILURE;
        if (channel_close(&channels[i]) == -1) result = EXIT_FAILURE;
    }
    if (stop_requested) result = EXIT_FAILURE;
    return result;
}
