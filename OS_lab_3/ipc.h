#ifndef LAB3_IPC_H
#define LAB3_IPC_H

#include <semaphore.h>
#include <stddef.h>

#define BUFFER_SIZE 4096
#define IPC_NAME_SIZE 32

/* No pointers: mappings in different processes can have different addresses. */
struct shared_block {
    size_t length;
    int end_of_input;
    unsigned char data[BUFFER_SIZE];
};

struct channel {
    char memory_name[IPC_NAME_SIZE];
    char empty_name[IPC_NAME_SIZE];
    char ready_name[IPC_NAME_SIZE];
    struct shared_block *block;
    sem_t *empty;
    sem_t *ready;
    int owns_memory;
    int owns_empty;
    int owns_ready;
};

void channel_init(struct channel *channel);
int channel_create(struct channel *channel, unsigned int number);
int channel_open(struct channel *channel, const char *memory_name,
                 const char *empty_name, const char *ready_name);
int channel_close(struct channel *channel);
int write_all(int fd, const void *buffer, size_t length);

#endif
