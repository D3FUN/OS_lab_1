#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    const char *text;
    const char *pattern;
    size_t pattern_length;
    size_t begin;
    size_t end;
    unsigned char *matches;
    size_t count;
} Task;

void *search(void *arg)
{
    Task *task = arg;
    task->count = 0;
    for (size_t i = task->begin; i < task->end; i++) {
        size_t j = 0;
        while (j < task->pattern_length && task->text[i + j] == task->pattern[j]) {
            j++;
        }
        if (j == task->pattern_length) {
            task->count++;
            if (task->matches != NULL) {
                task->matches[i] = 1;
            }
        }
    }
    return NULL;
}

char *read_file(const char *name, size_t *length)
{
    FILE *file = fopen(name, "rb");
    if (file == NULL) {
        perror(name);
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    char *text = malloc(size > 0 ? (size_t)size : 1);
    if (text == NULL) {
        fclose(file);
        return NULL;
    }
    size_t bytes = fread(text, 1, (size_t)size, file);
    int error = ferror(file);
    int close_error = fclose(file);
    if (bytes != (size_t)size || error || close_error != 0) {
        free(text);
        return NULL;
    }
    *length = bytes;
    return text;
}

int main(int argc, char *argv[])
{
    int limit = 0, quiet = 0, option;
    const char *filename = NULL;
    while ((option = getopt(argc, argv, "t:f:q")) != -1) {
        switch (option) {
        case 't': {
            char *end;
            errno = 0;
            long value = strtol(optarg, &end, 10);
            if (errno || end == optarg || *end != '\0' || value < 1 || value > INT_MAX) {
                fprintf(stderr, "Число потоков должно быть положительным целым\n");
                return 1;
            }
            limit = (int)value;
            break;
        }
        case 'f': filename = optarg; break;
        case 'q': quiet = 1; break;
        default: return 1;
        }
    }
    if (limit == 0 || argc - optind != (filename != NULL ? 1 : 2)) {
        fprintf(stderr, "Запуск: %s -t N [-q] ТЕКСТ ОБРАЗЕЦ\n", argv[0]);
        fprintf(stderr, "Или:    %s -t N [-q] -f ФАЙЛ ОБРАЗЕЦ\n", argv[0]);
        return 1;
    }
    const char *pattern = argv[argc - 1];
    size_t m = strlen(pattern);
    if (m == 0) {
        fprintf(stderr, "Образец не должен быть пустым\n");
        return 1;
    }
    char *file_text = NULL;
    const char *text;
    size_t n;
    if (filename != NULL) {
        file_text = read_file(filename, &n);
        if (file_text == NULL) {
            fprintf(stderr, "Не удалось прочитать файл\n");
            return 1;
        }
        text = file_text;
    } else {
        text = argv[optind];
        n = strlen(text);
    }

    size_t positions = m <= n ? n - m + 1 : 0;
    int p = limit;
    if (positions == 0) p = 1;
    else if (positions < (size_t)p) p = (int)positions;
    unsigned char *matches = NULL;
    if (!quiet && positions > 0) {
        matches = calloc(positions, 1);
        if (matches == NULL) {
            free(file_text);
            fprintf(stderr, "Не хватает памяти\n");
            return 1;
        }
    }
    struct timespec start, finish;
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        perror("clock_gettime");
        free(matches);
        free(file_text);
        return 1;
    }
    Task *tasks = calloc((size_t)p, sizeof(Task));
    pthread_t *threads = calloc((size_t)p, sizeof(pthread_t));
    if (tasks == NULL || threads == NULL) {
        free(tasks); free(threads); free(matches); free(file_text);
        fprintf(stderr, "Не хватает памяти\n");
        return 1;
    }
    size_t part = positions / (size_t)p;
    size_t extra = positions % (size_t)p;
    size_t begin = 0;
    for (int i = 0; i < p; i++) {
        size_t length = part + ((size_t)i < extra ? 1 : 0);
        /* Делим позиции начала, чтобы не терять совпадения на границах. */
        tasks[i] = (Task){text, pattern, m, begin, begin + length, matches, 0};
        begin += length;
    }
    int created = 0, failed = 0;
    for (int i = 0; i < p - 1; i++) {
        int error = pthread_create(&threads[i], NULL, search, &tasks[i]);
        if (error != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(error));
            failed = 1;
            break;
        }
        created++;
    }
    if (!failed) search(&tasks[p - 1]);
    for (int i = 0; i < created; i++) {
        int error = pthread_join(threads[i], NULL);
        if (error != 0) {
            fprintf(stderr, "pthread_join: %s\n", strerror(error));
            exit(1);
        }
    }
    size_t count = 0;
    for (int i = 0; i < p; i++) count += tasks[i].count;
    free(tasks);
    free(threads);
    if (clock_gettime(CLOCK_MONOTONIC, &finish) != 0) {
        perror("clock_gettime");
        failed = 1;
    }
    if (!failed) {
        double ms = (finish.tv_sec - start.tv_sec) * 1000.0
                  + (finish.tv_nsec - start.tv_nsec) / 1000000.0;
        if (!quiet) {
            for (size_t i = 0; i < positions; i++) {
                if (matches[i]) printf("%zu ", i);
            }
            printf("\n");
        }
        printf("Совпадений %zu\nПотоков %d\nВремя %.6f мс\n", count, p, ms);
        if (fflush(stdout) != 0 || ferror(stdout)) failed = 1;
    }
    free(matches);
    free(file_text);
    return failed;
}
