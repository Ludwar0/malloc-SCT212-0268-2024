#define _POSIX_C_SOURCE 200809L
#include "myalloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <string.h>

#define SLOTS 512
#define OPERATIONS 50000

static double elapsed(struct timespec a, struct timespec b)
{
    return (double)(b.tv_sec - a.tv_sec) + (double)(b.tv_nsec - a.tv_nsec) / 1e9;
}

static void run_custom(fit_policy_t policy, const char *label)
{
    void *slots[SLOTS] = {0};
    struct timespec start, end;
    srand(20261120);
    set_policy(policy);
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < OPERATIONS; i++) {
        int slot = rand() % SLOTS;
        if (slots[slot]) {
            my_free(slots[slot]); slots[slot] = NULL;
        } else {
            size_t n = 1U + (size_t)(rand() % 4096);
            slots[slot] = my_malloc(n);
            if (slots[slot]) memset(slots[slot], (unsigned char)slot, n);
        }
    }
    for (int i = 0; i < SLOTS; i++) if (slots[i]) my_free(slots[i]);
    clock_gettime(CLOCK_MONOTONIC, &end);
    heap_stats_t stats; heap_stats(&stats);
    double frag = stats.free_bytes ? 1.0 - (double)stats.largest_free / (double)stats.free_bytes : 0.0;
    printf("%s: time=%.6f s heap_bytes=%zu free_bytes=%zu free_blocks=%zu largest_free=%zu fragmentation=%.4f check=%s\n",
           label, elapsed(start, end), stats.heap_bytes, stats.free_bytes,
           stats.free_blocks, stats.largest_free, frag, heap_check() ? "OK" : "FAILED");
}

static void run_libc(void)
{
    void *slots[SLOTS] = {0};
    struct timespec start, end;
    srand(20261120);
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < OPERATIONS; i++) {
        int slot = rand() % SLOTS;
        if (slots[slot]) { free(slots[slot]); slots[slot] = NULL; }
        else {
            size_t n = 1U + (size_t)(rand() % 4096);
            slots[slot] = malloc(n);
            if (slots[slot]) memset(slots[slot], (unsigned char)slot, n);
        }
    }
    for (int i = 0; i < SLOTS; i++) free(slots[i]);
    clock_gettime(CLOCK_MONOTONIC, &end);
    printf("glibc malloc/free: time=%.6f s (allocator heap statistics are not exposed by this benchmark)\n", elapsed(start, end));
}

int main(void)
{
    puts("Fixed workload: 50,000 operations, 512 slots, sizes 1..4096, seed 20261120");
    run_custom(FIT_FIRST, "FIT_FIRST");
    run_custom(FIT_BEST, "FIT_BEST");
    run_libc();
    return 0;
}
