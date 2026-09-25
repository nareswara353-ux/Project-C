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

typedef struct SkipList SkipList;
typedef struct SkipNode SkipNode;

extern SkipList *skiplist_create(LsmComparator cmp, int max_level);
extern void skiplist_destroy(SkipList *list);
extern LsmStatus skiplist_insert(SkipList *list, LsmInternalKey key, LsmSlice value);
extern LsmStatus skiplist_find(SkipList *list, LsmSlice key, LsmSlice *out_value);
extern size_t skiplist_size(SkipList *list);
extern SkipNode *skiplist_begin(SkipList *list);
extern SkipNode *skiplist_next(SkipNode *node);
extern LsmSlice skiplist_node_key(SkipNode *node);
extern LsmSlice skiplist_node_value(SkipNode *node);

static int bytewise_cmp(const void *a, size_t la, const void *b, size_t lb) {
    size_t min_len = la < lb ? la : lb;
    int r = memcmp(a, b, min_len);
    if (r != 0)
        return r;
    if (la < lb)
        return -1;
    if (la > lb)
        return 1;
    return 0;
}

static void test_skiplist_insert_find(void) {
    SkipList *list = skiplist_create(bytewise_cmp, 16);
    CHECK(list != NULL);
    CHECK(skiplist_size(list) == 0);

    LsmInternalKey k1 = {lsm_slice_str("alpha"), 1};
    LsmInternalKey k2 = {lsm_slice_str("beta"), 2};
    LsmInternalKey k3 = {lsm_slice_str("gamma"), 3};
    CHECK(skiplist_insert(list, k1, lsm_slice_str("one")) == LSM_OK);
    CHECK(skiplist_insert(list, k2, lsm_slice_str("two")) == LSM_OK);
    CHECK(skiplist_insert(list, k3, lsm_slice_str("three")) == LSM_OK);
    CHECK(skiplist_size(list) == 3);

    LsmSlice out = {NULL, 0};
    CHECK(skiplist_find(list, lsm_slice_str("alpha"), &out) == LSM_OK);
    CHECK(out.len == 3);
    CHECK(memcmp(out.data, "one", 3) == 0);

    out = (LsmSlice){NULL, 0};
    CHECK(skiplist_find(list, lsm_slice_str("beta"), &out) == LSM_OK);
    CHECK(out.len == 3);
    CHECK(memcmp(out.data, "two", 3) == 0);

    out = (LsmSlice){NULL, 0};
    CHECK(skiplist_find(list, lsm_slice_str("gamma"), &out) == LSM_OK);
    CHECK(out.len == 5);
    CHECK(memcmp(out.data, "three", 5) == 0);

    out = (LsmSlice){NULL, 0};
    CHECK(skiplist_find(list, lsm_slice_str("missing"), &out) == LSM_ERR_NOT_FOUND);

    skiplist_destroy(list);
}

static void test_skiplist_traversal(void) {
    SkipList *list = skiplist_create(bytewise_cmp, 16);
    CHECK(list != NULL);

    const char *keys[] = {"aaa", "bbb", "ccc", "ddd", "eee"};
    for (size_t i = 0; i < 5; i++) {
        LsmInternalKey k = {lsm_slice_str(keys[i]), (SequenceNumber)(i + 1)};
        CHECK(skiplist_insert(list, k, lsm_slice_str("v")) == LSM_OK);
    }

    SkipNode *node = skiplist_begin(list);
    int count = 0;
    while (node != NULL) {
        LsmSlice k = skiplist_node_key(node);
        CHECK(k.len > 0);
        count++;
        node = skiplist_next(node);
    }
    CHECK(count == 5);

    skiplist_destroy(list);
}

static void test_skiplist_overwrite(void) {
    SkipList *list = skiplist_create(bytewise_cmp, 16);
    CHECK(list != NULL);

    LsmInternalKey k = {lsm_slice_str("key"), 1};
    CHECK(skiplist_insert(list, k, lsm_slice_str("first")) == LSM_OK);
    CHECK(skiplist_insert(list, k, lsm_slice_str("second")) == LSM_OK);
    CHECK(skiplist_size(list) == 1);

    LsmSlice out = {NULL, 0};
    CHECK(skiplist_find(list, lsm_slice_str("key"), &out) == LSM_OK);
    CHECK(out.len == 6);
    CHECK(memcmp(out.data, "second", 6) == 0);

    skiplist_destroy(list);
}

static void test_skiplist_many(void) {
    SkipList *list = skiplist_create(bytewise_cmp, 16);
    CHECK(list != NULL);

    char buf[32];
    for (int i = 0; i < 500; i++) {
        snprintf(buf, sizeof(buf), "key_%04d", i);
        LsmInternalKey k = {lsm_slice_str(buf), (SequenceNumber)(i + 1)};
        CHECK(skiplist_insert(list, k, lsm_slice_str("v")) == LSM_OK);
    }
    CHECK(skiplist_size(list) == 500);

    for (int i = 0; i < 500; i++) {
        snprintf(buf, sizeof(buf), "key_%04d", i);
        LsmSlice out = {NULL, 0};
        CHECK(skiplist_find(list, lsm_slice_str(buf), &out) == LSM_OK);
    }

    skiplist_destroy(list);
}

static void test_skiplist_null(void) {
    CHECK(skiplist_create(NULL, 4) != NULL);
    skiplist_destroy(NULL);
    CHECK(skiplist_find(NULL, lsm_slice_str("x"), NULL) == LSM_ERR_INVALID_ARG);
    CHECK(skiplist_size(NULL) == 0);
    CHECK(skiplist_begin(NULL) == NULL);
    CHECK(skiplist_next(NULL) == NULL);
    CHECK(skiplist_node_key(NULL).len == 0);
    CHECK(skiplist_node_value(NULL).len == 0);
}

int main(void) {
    test_skiplist_insert_find();
    test_skiplist_traversal();
    test_skiplist_overwrite();
    test_skiplist_many();
    test_skiplist_null();
    puts("test_skiplist: OK");
    return 0;
}
