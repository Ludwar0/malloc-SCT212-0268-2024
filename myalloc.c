#define _DEFAULT_SOURCE
#include "myalloc.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <inttypes.h>
#include <limits.h>

#define ALIGNMENT ((size_t)16)
#define MAGIC 0x4D59414C
#define CHUNK_SIZE ((size_t)64 * 1024)

typedef struct block {
    size_t size;
    struct block *next;
    int free;
    int magic;
} block_t;
#define HDR ((sizeof(block_t) + 15U) & ~(size_t)15U)

static block_t *head;
static block_t *tail;
static size_t obtained_bytes;
static fit_policy_t policy = FIT_FIRST;

static size_t align_up(size_t n)
{
    if (n > SIZE_MAX - 15U) return 0;
    return (n + 15U) & ~(size_t)15U;
}

static void *payload(block_t *b) { return (void *)((char *)b + HDR); }
static block_t *header(const void *p) { return (block_t *)((char *)p - HDR); }

static int adjacent(const block_t *a, const block_t *b)
{
    return (const char *)payload((block_t *)a) + a->size == (const char *)b;
}

static block_t *grow(size_t need)
{
    size_t amount = need > CHUNK_SIZE ? need : CHUNK_SIZE;
    void *current = sbrk(0);
    if (current == (void *)-1) return NULL;
    uintptr_t address = (uintptr_t)current;
    size_t padding = (size_t)((ALIGNMENT - (address % ALIGNMENT)) % ALIGNMENT);
    if (amount > SIZE_MAX - HDR - padding) return NULL;
    size_t total = padding + HDR + amount;
    void *base = sbrk((intptr_t)total);
    if (base == (void *)-1) return NULL;
    block_t *b = (block_t *)((char *)base + padding);
    b->size = amount;
    b->next = NULL;
    b->free = 1;
    b->magic = MAGIC;
    if (tail) tail->next = b; else head = b;
    tail = b;
    obtained_bytes += total;
    return b;
}

static block_t *find_fit(size_t need)
{
    block_t *chosen = NULL;
    for (block_t *b = head; b; b = b->next) {
        if (b->magic != MAGIC || !b->free || b->size < need) continue;
        if (policy == FIT_FIRST) return b;
        if (!chosen || b->size < chosen->size) chosen = b;
    }
    return chosen;
}

static void split_block(block_t *b, size_t need)
{
    if (b->size < need || b->size - need < HDR + ALIGNMENT) return;
    block_t *remainder = (block_t *)((char *)payload(b) + need);
    remainder->size = b->size - need - HDR;
    remainder->next = b->next;
    remainder->free = 1;
    remainder->magic = MAGIC;
    b->size = need;
    b->next = remainder;
    if (tail == b) tail = remainder;
}

void *my_malloc(size_t size)
{
    if (size == 0) return NULL;
    size_t need = align_up(size);
    if (need == 0) return NULL;
    block_t *b = find_fit(need);
    if (!b) b = grow(need);
    if (!b) return NULL;
    split_block(b, need);
    b->free = 0;
    return payload(b);
}

static void report_message(const char *message)
{
    (void)write(STDERR_FILENO, message, strlen(message));
}

static void coalesce(void)
{
    block_t *b = head;
    while (b && b->next) {
        block_t *next = b->next;
        if (b->free && next->free && adjacent(b, next)) {
            b->size += HDR + next->size;
            b->next = next->next;
            next->magic = 0;
            if (tail == next) tail = b;
        } else b = next;
    }
}

void my_free(void *ptr)
{
    if (!ptr) return;
    block_t *b = head;
    while (b && payload(b) != ptr) b = b->next;
    if (!b || b != header(ptr) || b->magic != MAGIC) { report_message("my_free: invalid pointer\n"); return; }
    if (b->free) { report_message("my_free: double free detected\n"); return; }
    b->free = 1;
    coalesce();
}

void *my_calloc(size_t nmemb, size_t size)
{
    if (nmemb != 0 && size > SIZE_MAX / nmemb) return NULL;
    size_t total = nmemb * size;
    void *p = my_malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void *my_realloc(void *ptr, size_t size)
{
    if (!ptr) return my_malloc(size);
    if (size == 0) { my_free(ptr); return NULL; }
    size_t need = align_up(size);
    if (!need) return NULL;
    block_t *b = head;
    while (b && payload(b) != ptr) b = b->next;
    if (!b || b->magic != MAGIC || b->free) { report_message("my_realloc: invalid pointer\n"); return NULL; }
    if (need <= b->size) { split_block(b, need); return ptr; }
    block_t *next = b->next;
    if (next && next->free && adjacent(b, next) && b->size + HDR + next->size >= need) {
        b->size += HDR + next->size;
        b->next = next->next;
        if (tail == next) tail = b;
        next->magic = 0;
        split_block(b, need);
        return ptr;
    }
    void *replacement = my_malloc(size);
    if (!replacement) return NULL;
    memcpy(replacement, ptr, b->size < size ? b->size : size);
    my_free(ptr);
    return replacement;
}

void heap_stats(heap_stats_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->heap_bytes = obtained_bytes;
    for (block_t *b = head; b; b = b->next) {
        out->blocks++;
        if (b->free) {
            out->free_blocks++;
            out->free_bytes += b->size;
            if (b->size > out->largest_free) out->largest_free = b->size;
        } else out->used_bytes += b->size;
    }
}

void heap_dump(void)
{
    char line[160];
    size_t index = 0;
    for (block_t *b = head; b; b = b->next, index++) {
        int n = snprintf(line, sizeof(line), "[%zu] %p size=%zu %s\n", index,
                         (void *)b, b->size, b->free ? "FREE" : "USED");
        if (n > 0) (void)write(STDOUT_FILENO, line, (size_t)n < sizeof(line) ? (size_t)n : sizeof(line));
    }
}

void set_policy(fit_policy_t p)
{
    if (p == FIT_FIRST || p == FIT_BEST) policy = p;
}

int heap_check(void)
{
    block_t *previous = NULL;
    size_t steps = 0;
    for (block_t *b = head; b; b = b->next) {
        if (++steps > 1000000 || b->magic != MAGIC || b->size % ALIGNMENT != 0) return 0;
        if ((uintptr_t)payload(b) % ALIGNMENT != 0) return 0;
        if (previous) {
            if ((uintptr_t)previous >= (uintptr_t)b) return 0;
            if ((uintptr_t)payload(previous) + previous->size > (uintptr_t)b) return 0;
            if (previous->free && b->free && adjacent(previous, b)) return 0;
        }
        previous = b;
    }
    return previous == tail && ((head == NULL) == (tail == NULL));
}
