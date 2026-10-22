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

static LsmIterator *make_array_iter(const char **keys, const char **values, size_t count) {
    ArrayIterState *state = malloc(sizeof(ArrayIterState));
    if (!state)
        return NULL;
    state->keys = keys;
    state->values = values;
    state->count = count;
    state->index = 0;
    return lsm_iterator_from_impl(&array_ops, state);
}

static void test_iterator_from_impl(void) {
    static const char *keys[] = {"a", "b", "c"};
    static const char *values[] = {"1", "2", "3"};

    LsmIterator *it = make_array_iter(keys, values, 3);
    CHECK(it != NULL);
    CHECK(lsm_iterator_valid(it) == true);

    LsmSlice k = lsm_iterator_key(it);
    CHECK(k.len == 1);
    CHECK(memcmp(k.data, "a", 1) == 0);

    LsmSlice v = lsm_iterator_value(it);
    CHECK(v.len == 1);
    CHECK(memcmp(v.data, "1", 1) == 0);

    lsm_iterator_next(it);
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

static void test_merge_two_sorted(void) {
    static const char *keys1[] = {"b", "d", "f"};
    static const char *vals1[] = {"vb", "vd", "vf"};
    static const char *keys2[] = {"a", "c", "e"};
    static const char *vals2[] = {"wa", "wc", "we"};

    LsmIterator *i1 = make_array_iter(keys1, vals1, 3);
    LsmIterator *i2 = make_array_iter(keys2, vals2, 3);
    CHECK(i1 != NULL);
    CHECK(i2 != NULL);

    LsmIterator *arr[2] = {i1, i2};
    LsmIterator *merged = lsm_merge_iterator(arr, 2);
    CHECK(merged != NULL);

    static const char *expected[] = {"a", "b", "c", "d", "e", "f"};
    size_t expected_count = 6;
    size_t i = 0;
    while (lsm_iterator_valid(merged)) {
        LsmSlice k = lsm_iterator_key(merged);
        CHECK(i < expected_count);
        CHECK(k.len == 1);
        CHECK(memcmp(k.data, expected[i], 1) == 0);
        i++;
        lsm_iterator_next(merged);
    }
    CHECK(i == expected_count);

    lsm_iterator_destroy(merged);
}

static void test_concat_two(void) {
    static const char *keys1[] = {"a", "b"};
    static const char *vals1[] = {"v1", "v2"};
    static const char *keys2[] = {"c", "d"};
    static const char *vals2[] = {"v3", "v4"};

    LsmIterator *i1 = make_array_iter(keys1, vals1, 2);
    LsmIterator *i2 = make_array_iter(keys2, vals2, 2);
    CHECK(i1 != NULL);
    CHECK(i2 != NULL);

    LsmIterator *arr[2] = {i1, i2};
    LsmIterator *concat = lsm_concat_iterator(arr, 2);
    CHECK(concat != NULL);

    static const char *expected[] = {"a", "b", "c", "d"};
    size_t expected_count = 4;
    size_t i = 0;
    while (lsm_iterator_valid(concat)) {
        LsmSlice k = lsm_iterator_key(concat);
        CHECK(i < expected_count);
        CHECK(k.len == 1);
        CHECK(memcmp(k.data, expected[i], 1) == 0);
        i++;
        lsm_iterator_next(concat);
    }
    CHECK(i == expected_count);

    lsm_iterator_destroy(concat);
}

static void test_iterator_tree_stub(void) {
    LsmOptions o = lsm_options_default();
    LsmTree *tree = NULL;
    CHECK(lsm_open(&o, &tree) == LSM_OK);

    LsmIterator *it = NULL;
    CHECK(lsm_iterator(tree, &it) == LSM_OK);
    CHECK(it != NULL);
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
    test_merge_two_sorted();
    test_concat_two();
    test_iterator_tree_stub();
    puts("test_iterator: OK");
    return 0;
}
