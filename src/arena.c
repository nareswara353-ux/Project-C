#include "internal/arena.h"

#include "lsm_types.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ARENA_ALIGN 8u

static size_t align_up(size_t value) {
    return (value + (ARENA_ALIGN - 1)) & ~(size_t)(ARENA_ALIGN - 1);
}

LsmStatus lsm_arena_init(LsmArena *arena, size_t block_size) {
    if (!arena || block_size == 0)
        return LSM_ERR_INVALID_ARG;
    arena->head = NULL;
    arena->block_size = block_size;
    arena->total_allocated = 0;
    return LSM_OK;
}

void lsm_arena_destroy(LsmArena *arena) {
    if (!arena)
        return;
    LsmArenaBlock *cur = arena->head;
    while (cur) {
        LsmArenaBlock *next = cur->next;
        free(cur);
        cur = next;
    }
    arena->head = NULL;
    arena->total_allocated = 0;
}

void lsm_arena_reset(LsmArena *arena) {
    if (!arena)
        return;
    LsmArenaBlock *cur = arena->head;
    while (cur) {
        LsmArenaBlock *next = cur->next;
        free(cur);
        cur = next;
    }
    arena->head = NULL;
    arena->total_allocated = 0;
}

void *lsm_arena_alloc(LsmArena *arena, size_t size) {
    if (!arena || size == 0)
        return NULL;
    size_t aligned = align_up(size);
    LsmArenaBlock *b = arena->head;
    if (!b || b->capacity - b->used < aligned) {
        size_t cap = arena->block_size > aligned ? arena->block_size : aligned;
        LsmArenaBlock *nb = malloc(sizeof(LsmArenaBlock) + cap);
        if (!nb)
            return NULL;
        nb->next = arena->head;
        nb->used = 0;
        nb->capacity = cap;
        arena->head = nb;
        b = nb;
    }
    void *ptr = b->data + b->used;
    b->used += aligned;
    arena->total_allocated += aligned;
    return ptr;
}

size_t lsm_arena_total(const LsmArena *arena) {
    if (!arena)
        return 0;
    return arena->total_allocated;
}
