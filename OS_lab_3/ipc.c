#define _POSIX_C_SOURCE 200809L
#include "ipc.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

void channel_init(struct channel *channel) {
    memset(channel, 0, sizeof(*channel));
    channel->block = MAP_FAILED;
    channel->empty = SEM_FAILED;
    channel->ready = SEM_FAILED;
}

static int map_memory(struct channel *channel, int fd) {
    channel->block = mmap(NULL, sizeof(struct shared_block),
                          PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    int saved_errno = errno;
    int close_result = close(fd);
    if (channel->block == MAP_FAILED) {
        errno = saved_errno;
        return -1;
    }
    return close_result;
}

static int make_name(char *name, unsigned int number, char kind,
                     unsigned long stamp) {
    int length = snprintf(name, IPC_NAME_SIZE, "/l3_%lx_%lx_%u%c",
                          (unsigned long)getpid(), stamp, number, kind);
    if (length < 0 || length >= IPC_NAME_SIZE) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}

int channel_create(struct channel *channel, unsigned int number) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) return -1;
    unsigned long stamp = (unsigned long)now.tv_nsec;
    if (make_name(channel->memory_name, number, 'm', stamp) == -1 ||
        make_name(channel->empty_name, number, 'e', stamp) == -1 ||
        make_name(channel->ready_name, number, 'r', stamp) == -1) return -1;

    int fd = shm_open(channel->memory_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd == -1) return -1;
    channel->owns_memory = 1;
    if (ftruncate(fd, (off_t)sizeof(struct shared_block)) == -1) {
        int saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }
    if (map_memory(channel, fd) == -1) return -1;
    channel->empty = sem_open(channel->empty_name, O_CREAT | O_EXCL, 0600, 1);
    if (channel->empty == SEM_FAILED) return -1;
    channel->owns_empty = 1;
    channel->ready = sem_open(channel->ready_name, O_CREAT | O_EXCL, 0600, 0);
    if (channel->ready == SEM_FAILED) return -1;
    channel->owns_ready = 1;
    return 0;
}

int channel_open(struct channel *channel, const char *memory_name,
                 const char *empty_name, const char *ready_name) {
    int fd = shm_open(memory_name, O_RDWR, 0);
    if (fd == -1) return -1;
    struct stat info;
    int result = fstat(fd, &info);
    /* Darwin rounds a shared object's size up to a whole memory page. */
    if (result == -1 || info.st_size < (off_t)sizeof(struct shared_block)) {
        int saved_errno = result == -1 ? errno : EINVAL;
        close(fd);
        errno = saved_errno;
        return -1;
    }
    if (map_memory(channel, fd) == -1) return -1;
    channel->empty = sem_open(empty_name, 0);
    if (channel->empty == SEM_FAILED) return -1;
    channel->ready = sem_open(ready_name, 0);
    return channel->ready == SEM_FAILED ? -1 : 0;
}

int channel_close(struct channel *channel) {
    int result = 0;
    if (channel->block != MAP_FAILED &&
        munmap(channel->block, sizeof(struct shared_block)) == -1) {
        perror("munmap");
        result = -1;
    }
    if (channel->empty != SEM_FAILED && sem_close(channel->empty) == -1) {
        perror("sem_close(empty)");
        result = -1;
    }
    if (channel->ready != SEM_FAILED && sem_close(channel->ready) == -1) {
        perror("sem_close(ready)");
        result = -1;
    }
    if (channel->owns_memory && shm_unlink(channel->memory_name) == -1) {
        perror("shm_unlink");
        result = -1;
    }
    if (channel->owns_empty && sem_unlink(channel->empty_name) == -1) {
        perror("sem_unlink(empty)");
        result = -1;
    }
    if (channel->owns_ready && sem_unlink(channel->ready_name) == -1) {
        perror("sem_unlink(ready)");
        result = -1;
    }
    channel_init(channel);
    return result;
}

int write_all(int fd, const void *buffer, size_t length) {
    const unsigned char *bytes = buffer;
    while (length > 0) {
        ssize_t written = write(fd, bytes, length);
        if (written == -1 && errno == EINTR) continue;
        if (written <= 0) {
            if (written == 0) errno = EIO;
            return -1;
        }
        bytes += written;
        length -= (size_t)written;
    }
    return 0;
}
