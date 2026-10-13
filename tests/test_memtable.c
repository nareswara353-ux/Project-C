#include "lsm_tree.h"
#include "lsm_types.h"
#include "memtable.h"

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

static void test_memtable_put_get(void) {
    void *mt = memtable_create(bytewise_cmp);
    CHECK(mt != NULL);

    LsmInternalKey k1 = {lsm_slice_str("alpha"), 1};
    LsmInternalKey k2 = {lsm_slice_str("beta"), 2};
    CHECK(memtable_put(mt, k1, lsm_slice_str("one")) == LSM_OK);
    CHECK(memtable_put(mt, k2, lsm_slice_str("two")) == LSM_OK);

    LsmSlice out = {NULL, 0};
    CHECK(memtable_get(mt, lsm_slice_str("alpha"), &out) == LSM_OK);
    CHECK(out.len == 3);
    CHECK(memcmp(out.data, "one", 3) == 0);

    out = (LsmSlice){NULL, 0};
    CHECK(memtable_get(mt, lsm_slice_str("beta"), &out) == LSM_OK);
    CHECK(out.len == 3);
    CHECK(memcmp(out.data, "two", 3) == 0);

    out = (LsmSlice){NULL, 0};
    CHECK(memtable_get(mt, lsm_slice_str("gamma"), &out) == LSM_ERR_NOT_FOUND);

    memtable_destroy(mt);
}

static void test_memtable_size(void) {
    void *mt = memtable_create(bytewise_cmp);
    CHECK(mt != NULL);
    size_t before = memtable_size(mt);

    LsmInternalKey k = {lsm_slice_str("key"), 1};
    CHECK(memtable_put(mt, k, lsm_slice_str("value")) == LSM_OK);
    size_t after = memtable_size(mt);
    CHECK(after > before);

    memtable_destroy(mt);
}

static void test_memtable_iterator(void) {
    void *mt = memtable_create(bytewise_cmp);
    CHECK(mt != NULL);

    LsmInternalKey k1 = {lsm_slice_str("aaa"), 1};
    LsmInternalKey k2 = {lsm_slice_str("bbb"), 2};
    LsmInternalKey k3 = {lsm_slice_str("ccc"), 3};
    CHECK(memtable_put(mt, k1, lsm_slice_str("v1")) == LSM_OK);
    CHECK(memtable_put(mt, k2, lsm_slice_str("v2")) == LSM_OK);
    CHECK(memtable_put(mt, k3, lsm_slice_str("v3")) == LSM_OK);

    int seen = 0;
    for (void *it = memtable_iterator(mt); memtable_iterator_valid(it);) {
        CHECK(memtable_iterator_key(it).len > 0);
        CHECK(memtable_iterator_value(it).len > 0);
        seen++;
        memtable_iterator_next(it);
        it = memtable_iterator(mt);
        for (int i = 0; i < seen; i++)
            memtable_iterator_next(memtable_iterator(mt));
    }
    CHECK(seen == 3);

    memtable_destroy(mt);
}

static void test_memtable_null(void) {
    CHECK(memtable_size(NULL) == 0);
    CHECK(memtable_iterator(NULL) == NULL);
    CHECK(memtable_iterator_valid(NULL) == false);
}

int main(void) {
    test_memtable_put_get();
    test_memtable_size();
    test_memtable_iterator();
    test_memtable_null();
    puts("test_memtable: OK");
    return 0;
}
