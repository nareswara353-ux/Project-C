#include "internal/arena.h"
#include "lsm_types.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check_impl(int ok, const char *expr, const char *file, int line) {
    if (!ok) {
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", file, line, expr);
        exit(1);
    }
}

#define CHECK(cond) check_impl((cond) ? 1 : 0, #cond, __FILE__, __LINE__)

static void test_arena_init_destroy(void) {
    LsmArena a;
    CHECK(lsm_arena_init(&a, 4096) == LSM_OK);
    CHECK(a.head == NULL);
    CHECK(a.block_size == 4096);
    CHECK(lsm_arena_total(&a) == 0);
    lsm_arena_destroy(&a);
    CHECK(a.head == NULL);
    CHECK(lsm_arena_total(&a) == 0);
}

static void test_arena_basic_alloc(void) {
    LsmArena a;
    CHECK(lsm_arena_init(&a, 4096) == LSM_OK);

    void *p1 = lsm_arena_alloc(&a, 16);
    CHECK(p1 != NULL);
    void *p2 = lsm_arena_alloc(&a, 32);
    CHECK(p2 != NULL);
    CHECK(p1 != p2);

    memset(p1, 0xAA, 16);
    memset(p2, 0xBB, 32);
    uint8_t *b1 = p1;
    uint8_t *b2 = p2;
    CHECK(b1[0] == 0xAA && b1[15] == 0xAA);
    CHECK(b2[0] == 0xBB && b2[31] == 0xBB);

    CHECK(lsm_arena_total(&a) >= 48);

    lsm_arena_destroy(&a);
}

static void test_arena_alignment(void) {
    LsmArena a;
    CHECK(lsm_arena_init(&a, 1024) == LSM_OK);

    for (int i = 0; i < 32; i++) {
        void *p = lsm_arena_alloc(&a, (size_t)(i + 1));
        CHECK(p != NULL);
        uintptr_t addr = (uintptr_t)p;
        CHECK((addr % 8) == 0);
    }

    lsm_arena_destroy(&a);
}

static void test_arena_large_alloc(void) {
    LsmArena a;
    CHECK(lsm_arena_init(&a, 128) == LSM_OK);

    void *p = lsm_arena_alloc(&a, 10000);
    CHECK(p != NULL);
    memset(p, 0xCC, 10000);
    CHECK(lsm_arena_total(&a) >= 10000);

    lsm_arena_destroy(&a);
}

static void test_arena_multi_block(void) {
    LsmArena a;
    CHECK(lsm_arena_init(&a, 256) == LSM_OK);

    void *prev = NULL;
    for (int i = 0; i < 100; i++) {
        void *p = lsm_arena_alloc(&a, 100);
        CHECK(p != NULL);
        CHECK(p != prev);
        prev = p;
    }

    int blocks = 0;
    for (LsmArenaBlock *b = a.head; b; b = b->next)
        blocks++;
    CHECK(blocks > 1);

    lsm_arena_destroy(&a);
}

static void test_arena_reset(void) {
    LsmArena a;
    CHECK(lsm_arena_init(&a, 256) == LSM_OK);

    for (int i = 0; i < 10; i++)
        CHECK(lsm_arena_alloc(&a, 64) != NULL);
    CHECK(lsm_arena_total(&a) > 0);
    CHECK(a.head != NULL);

    lsm_arena_reset(&a);
    CHECK(a.head == NULL);
    CHECK(lsm_arena_total(&a) == 0);

    void *p = lsm_arena_alloc(&a, 64);
    CHECK(p != NULL);

    lsm_arena_destroy(&a);
}

static void test_arena_null(void) {
    CHECK(lsm_arena_init(NULL, 1024) == LSM_ERR_INVALID_ARG);
    CHECK(lsm_arena_alloc(NULL, 16) == NULL);

    LsmArena a;
    CHECK(lsm_arena_init(&a, 1024) == LSM_OK);
    CHECK(lsm_arena_alloc(&a, 0) == NULL);
    lsm_arena_destroy(&a);

    lsm_arena_destroy(NULL);
    lsm_arena_reset(NULL);
    CHECK(lsm_arena_total(NULL) == 0);
}

int main(void) {
    test_arena_init_destroy();
    test_arena_basic_alloc();
    test_arena_alignment();
    test_arena_large_alloc();
    test_arena_multi_block();
    test_arena_reset();
    test_arena_null();
    puts("test_arena: OK");
    return 0;
}
