#include "lsm_iterator.h"
#include "lsm_tree.h"
#include "lsm_types.h"

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

typedef struct {
    const char **keys;
    const char **values;
    size_t count;
    size_t index;
} ArrayIterState;

static bool array_valid(void *state) {
    ArrayIterState *s = state;
    return s->index < s->count;
}

static void array_next(void *state) {
    ArrayIterState *s = state;
    s->index++;
}

static LsmSlice array_key(void *state) {
    ArrayIterState *s = state;
    if (s->index >= s->count)
        return (LsmSlice){NULL, 0};
    return lsm_slice_str(s->keys[s->index]);
}

static LsmSlice array_value(void *state) {
    ArrayIterState *s = state;
    if (s->index >= s->count)
        return (LsmSlice){NULL, 0};
    return lsm_slice_str(s->values[s->index]);
}

static void array_destroy(void *state) {
    free(state);
}

static const LsmIteratorOps array_ops = {
    .valid = array_valid,
    .next = array_next,
    .key = array_key,
    .value = array_value,
    .destroy = array_destroy,
};

static void test_iterator_from_impl(void) {
    static const char *keys[] = {"a", "b", "c"};
    static const char *values[] = {"1", "2", "3"};

    ArrayIterState *state = malloc(sizeof(ArrayIterState));
    CHECK(state != NULL);
    state->keys = keys;
    state->values = values;
    state->count = 3;
    state->index = 0;

    LsmIterator *it = lsm_iterator_from_impl(&array_ops, state);
    CHECK(it != NULL);
    CHECK(lsm_iterator_valid(it) == true);

    LsmSlice k = lsm_iterator_key(it);
    CHECK(k.len == 1);
    CHECK(memcmp(k.data, "a", 1) == 0);

    LsmSlice v = lsm_iterator_value(it);
    CHECK(v.len == 1);
    CHECK(memcmp(v.data, "1", 1) == 0);

    lsm_iterator_next(it);
    CHECK(lsm_iterator_valid(it) == true);
    k = lsm_iterator_key(it);
    CHECK(memcmp(k.data, "b", 1) == 0);

    lsm_iterator_next(it);
    lsm_iterator_next(it);
    CHECK(lsm_iterator_valid(it) == false);

    lsm_iterator_destroy(it);
}

static void test_iterator_null_impl(void) {
    LsmIterator *it = lsm_iterator_from_impl(NULL, NULL);
    CHECK(it != NULL);
    CHECK(lsm_iterator_valid(it) == false);
    CHECK(lsm_iterator_key(it).len == 0);
    CHECK(lsm_iterator_value(it).len == 0);
    lsm_iterator_next(it);
    lsm_iterator_destroy(it);
}

static void test_iterator_tree_stub(void) {
    LsmOptions o = lsm_options_default();
    LsmTree *tree = NULL;
    CHECK(lsm_open(&o, &tree) == LSM_OK);

    LsmIterator *it = NULL;
    CHECK(lsm_iterator(tree, &it) == LSM_OK);
    CHECK(lsm_iterator_valid(it) == false);
    lsm_iterator_next(it);
    CHECK(lsm_iterator_key(it).len == 0);
    CHECK(lsm_iterator_value(it).len == 0);
    lsm_iterator_destroy(it);

    lsm_close(tree);
    lsm_options_destroy(&o);
}

int main(void) {
    test_iterator_from_impl();
    test_iterator_null_impl();
    test_iterator_tree_stub();
    puts("test_iterator: OK");
    return 0;
}
