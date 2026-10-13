#include "lsm_tree.h"
#include "lsm_types.h"
#include "memtable.h"
#include "wal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static void test_wal_reader_replay(void) {
    const char *wal_path = "/tmp/lsm_test_wal_reader.log";
    unlink(wal_path);

    Wal *wal = NULL;
    CHECK(wal_open(wal_path, false, &wal) == LSM_OK);
    CHECK(wal_append_put(wal, lsm_slice_str("k1"), lsm_slice_str("v1"), 1) == LSM_OK);
    CHECK(wal_append_put(wal, lsm_slice_str("k2"), lsm_slice_str("v2"), 2) == LSM_OK);
    CHECK(wal_append_put(wal, lsm_slice_str("k3"), lsm_slice_str("v3"), 3) == LSM_OK);
    CHECK(wal_sync(wal) == LSM_OK);
    wal_close(wal);

    void *mt = memtable_create(bytewise_cmp);
    CHECK(mt != NULL);

    CHECK(wal_replay_into_memtable(wal_path, mt) == LSM_OK);

    LsmSlice out = {NULL, 0};
    CHECK(memtable_get(mt, lsm_slice_str("k1"), &out) == LSM_OK);
    CHECK(out.len == 2);
    CHECK(memcmp(out.data, "v1", 2) == 0);

    out = (LsmSlice){NULL, 0};
    CHECK(memtable_get(mt, lsm_slice_str("k2"), &out) == LSM_OK);
    CHECK(out.len == 2);
    CHECK(memcmp(out.data, "v2", 2) == 0);

    out = (LsmSlice){NULL, 0};
    CHECK(memtable_get(mt, lsm_slice_str("k3"), &out) == LSM_OK);
    CHECK(out.len == 2);
    CHECK(memcmp(out.data, "v3", 2) == 0);

    memtable_destroy(mt);
    unlink(wal_path);
}

static void test_wal_reader_empty(void) {
    const char *wal_path = "/tmp/lsm_test_wal_reader_empty.log";
    unlink(wal_path);

    Wal *wal = NULL;
    CHECK(wal_open(wal_path, false, &wal) == LSM_OK);
    wal_close(wal);

    void *mt = memtable_create(bytewise_cmp);
    CHECK(mt != NULL);

    CHECK(wal_replay_into_memtable(wal_path, mt) == LSM_OK);

    LsmSlice out = {NULL, 0};
    CHECK(memtable_get(mt, lsm_slice_str("anything"), &out) == LSM_ERR_NOT_FOUND);

    memtable_destroy(mt);
    unlink(wal_path);
}

static void test_wal_reader_null_args(void) {
    CHECK(wal_replay_into_memtable(NULL, NULL) == LSM_ERR_INVALID_ARG);

    void *mt = memtable_create(bytewise_cmp);
    CHECK(mt != NULL);
    CHECK(wal_replay_into_memtable(NULL, mt) == LSM_ERR_INVALID_ARG);
    CHECK(wal_replay_into_memtable("/tmp/nonexistent.wal", mt) != LSM_OK);
    memtable_destroy(mt);
}

int main(void) {
    test_wal_reader_replay();
    test_wal_reader_empty();
    test_wal_reader_null_args();
    puts("test_wal_reader: OK");
    return 0;
}
