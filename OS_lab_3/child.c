#define _POSIX_C_SOURCE 200809L
#include "ipc.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int is_vowel(unsigned char character) {
    return character == 'a' || character == 'e' || character == 'i' ||
           character == 'o' || character == 'u' || character == 'A' ||
           character == 'E' || character == 'I' || character == 'O' ||
           character == 'U';
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s /memory /empty /ready\n", argv[0]);
        return EXIT_FAILURE;
    }
    struct channel channel;
    channel_init(&channel);
    int result = EXIT_FAILURE;
    if (channel_open(&channel, argv[1], argv[2], argv[3]) == -1) {
        perror("child: open IPC");
        goto cleanup;
    }
    for (;;) {
        int status;
        do {
            status = sem_wait(channel.ready);
        } while (status == -1 && errno == EINTR);
        if (status == -1) {
            perror("child: sem_wait");
            break;
        }
        size_t length = channel.block->length;
        int end_of_input = channel.block->end_of_input;
        if (length > BUFFER_SIZE || (end_of_input != 0 && end_of_input != 1) ||
            (end_of_input && length != 0)) {
            fprintf(stderr, "child: invalid shared-memory message\n");
            break;
        }
        unsigned char output[BUFFER_SIZE];
        size_t output_length = 0;
        for (size_t i = 0; i < length; ++i) {
            if (!is_vowel(channel.block->data[i])) {
                output[output_length++] = channel.block->data[i];
            }
        }
        if (write_all(STDOUT_FILENO, output, output_length) == -1) {
            perror("child: write output");
            break;
        }
        if (sem_post(channel.empty) == -1) {
            perror("child: sem_post");
            break;
        }
        if (end_of_input) {
            result = EXIT_SUCCESS;
            break;
        }
    }
cleanup:
    if (channel_close(&channel) == -1) result = EXIT_FAILURE;
    /* Report errors delayed until close (for example, a full filesystem). */
    if (close(STDOUT_FILENO) == -1) {
        perror("child: close output");
        result = EXIT_FAILURE;
    }
    return result;
}
