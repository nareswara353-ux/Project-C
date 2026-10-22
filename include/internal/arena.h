#ifndef LSM_INTERNAL_ARENA_H
#define LSM_INTERNAL_ARENA_H

#include "lsm_types.h"

#include <stddef.h>
#include <stdint.h>

typedef struct LsmArenaBlock {
    struct LsmArenaBlock *next;
    size_t used;
    size_t capacity;
    uint8_t data[];
} LsmArenaBlock;

typedef struct {
    LsmArenaBlock *head;
    size_t block_size;
    size_t total_allocated;
} LsmArena;

LsmStatus lsm_arena_init(LsmArena *arena, size_t block_size);
void lsm_arena_destroy(LsmArena *arena);
void *lsm_arena_alloc(LsmArena *arena, size_t size);
void lsm_arena_reset(LsmArena *arena);
size_t lsm_arena_total(const LsmArena *arena);

#endif
