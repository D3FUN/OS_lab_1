#include "commands.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef LIB_EXT
#define LIB_EXT ".so"
#endif

static const char *const paths[] = {"./libtrial" LIB_EXT, "./libsieve" LIB_EXT};
static void *handle;
static Contracts contracts;
static int current;

_Static_assert(sizeof contracts.count == sizeof(void *), "Unsupported function pointer size");
_Static_assert(sizeof contracts.approximate_e == sizeof(void *), "Unsupported function pointer size");

static int load_symbol(void *library, const char *name, void *destination)
{
    dlerror();
    void *address = dlsym(library, name);
    const char *error = dlerror();
    if (error != NULL || address == NULL) {
        fprintf(stderr, "dlsym %s: %s\n", name, error != NULL ? error : "null address");
        return -1;
    }
    /* On Linux and macOS dlsym returns the function address in the same representation. */
    memcpy(destination, &address, sizeof address);
    return 0;
}

static void close_library(void *library)
{
    if (dlclose(library) != 0) {
        fprintf(stderr, "dlclose: %s\n", dlerror());
    }
}

static int open_library(int index)
{
    void *next = dlopen(paths[index], RTLD_NOW | RTLD_LOCAL);
    if (next == NULL) {
        fprintf(stderr, "dlopen %s: %s\n", paths[index], dlerror());
        return -1;
    }

    Contracts loaded;
    if (load_symbol(next, "prime_count", &loaded.count) != 0 ||
        load_symbol(next, "e", &loaded.approximate_e) != 0) {
        close_library(next);
        return -1;
    }

    /* Preserve the old library until both new functions are available. */
    void *previous = handle;
    contracts = loaded;
    handle = next;
    current = index;
    if (previous != NULL) {
        close_library(previous);
    }
    return 0;
}

static void switch_library(void)
{
    open_library(1 - current);
}

int main(void)
{
    if (open_library(0) != 0) {
        return EXIT_FAILURE;
    }
    int status = run_commands(&contracts, switch_library);
    close_library(handle);
    return status;
}
