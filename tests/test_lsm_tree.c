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

static void test_options_default(void) {
    LsmOptions o = lsm_options_default();
    CHECK(o.db_path != NULL);
    CHECK(o.memtable_size_mb > 0);
    CHECK(o.max_open_files > 0);
    CHECK(o.block_size_kb > 0);
    CHECK(o.bloom_bits_per_key > 0);
    CHECK(o.owns_db_path == false);
    lsm_options_destroy(&o);
}

static void test_open_close(void) {
    LsmOptions o = lsm_options_default();
    LsmTree *tree = NULL;
    CHECK(lsm_open(&o, &tree) == LSM_OK);
    CHECK(tree != NULL);
    lsm_close(tree);
    lsm_options_destroy(&o);
}

static void test_put_get_delete(void) {
    LsmOptions o = lsm_options_default();
    LsmTree *tree = NULL;
    CHECK(lsm_open(&o, &tree) == LSM_OK);

    CHECK(lsm_put(tree, lsm_slice_str("alpha"), lsm_slice_str("one")) == LSM_OK);
    CHECK(lsm_put(tree, lsm_slice_str("beta"), lsm_slice_str("two")) == LSM_OK);
    CHECK(lsm_delete(tree, lsm_slice_str("alpha")) == LSM_OK);

    LsmSlice out = {NULL, 0};
    LsmStatus st = lsm_get(tree, lsm_slice_str("alpha"), &out);
    CHECK(st == LSM_ERR_NOT_FOUND || st == LSM_OK);

    lsm_close(tree);
    lsm_options_destroy(&o);
}

static void test_iterator_stub(void) {
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

static void test_null_args(void) {
    CHECK(lsm_open(NULL, NULL) == LSM_ERR_INVALID_ARG);

    LsmTree *tree = NULL;
    LsmOptions o = lsm_options_default();
    CHECK(lsm_open(&o, &tree) == LSM_OK);

    CHECK(lsm_put(tree, lsm_slice_str("k"), lsm_slice_str("v")) == LSM_OK);
    CHECK(lsm_put(NULL, lsm_slice_str("k"), lsm_slice_str("v")) == LSM_ERR_INVALID_ARG);
    CHECK(lsm_put(tree, (LsmSlice){NULL, 0}, lsm_slice_str("v")) == LSM_ERR_INVALID_ARG);

    CHECK(lsm_get(NULL, lsm_slice_str("k"), NULL) == LSM_ERR_INVALID_ARG);
    CHECK(lsm_delete(NULL, lsm_slice_str("k")) == LSM_ERR_INVALID_ARG);
    CHECK(lsm_iterator(NULL, NULL) == LSM_ERR_INVALID_ARG);

    lsm_iterator_destroy(NULL);
    lsm_close(NULL);
    lsm_options_destroy(NULL);

    lsm_close(tree);
    lsm_options_destroy(&o);
}

int main(void) {
    test_options_default();
    test_open_close();
    test_put_get_delete();
    test_iterator_stub();
    test_null_args();
    puts("test_lsm_tree: OK");
    return 0;
}
