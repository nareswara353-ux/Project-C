#include "lsm_tree.h"
#include "lsm_types.h"

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

typedef struct {
    LsmSlice key;
    LsmSlice value;
} SstableEntry;

extern LsmStatus sstable_build(const char *path, const SstableEntry *entries, size_t count);
extern LsmStatus sstable_open(const char *path, void **out);
extern LsmStatus sstable_get(void *handle, LsmSlice key, LsmSlice *out_value);
extern void sstable_close(void *handle);

static void test_sstable_roundtrip(void) {
    const char *path = "/tmp/lsm_test_sstable_roundtrip.sst";
    unlink(path);

    SstableEntry entries[3];
    entries[0].key = lsm_slice_str("alpha");
    entries[0].value = lsm_slice_str("one");
    entries[1].key = lsm_slice_str("beta");
    entries[1].value = lsm_slice_str("two");
    entries[2].key = lsm_slice_str("gamma");
    entries[2].value = lsm_slice_str("three");

    CHECK(sstable_build(path, entries, 3) == LSM_OK);

    void *sst = NULL;
    CHECK(sstable_open(path, &sst) == LSM_OK);
    CHECK(sst != NULL);

    LsmSlice out = {NULL, 0};
    CHECK(sstable_get(sst, lsm_slice_str("alpha"), &out) == LSM_OK);
    CHECK(out.len == 3);
    CHECK(memcmp(out.data, "one", 3) == 0);

    out = (LsmSlice){NULL, 0};
    CHECK(sstable_get(sst, lsm_slice_str("beta"), &out) == LSM_OK);
    CHECK(out.len == 3);
    CHECK(memcmp(out.data, "two", 3) == 0);

    out = (LsmSlice){NULL, 0};
    CHECK(sstable_get(sst, lsm_slice_str("gamma"), &out) == LSM_OK);
    CHECK(out.len == 5);
    CHECK(memcmp(out.data, "three", 5) == 0);

    out = (LsmSlice){NULL, 0};
    CHECK(sstable_get(sst, lsm_slice_str("missing"), &out) == LSM_ERR_NOT_FOUND);

    sstable_close(sst);
    unlink(path);
}

static void test_sstable_empty(void) {
    const char *path = "/tmp/lsm_test_sstable_empty.sst";
    unlink(path);

    CHECK(sstable_build(path, NULL, 0) == LSM_OK);

    void *sst = NULL;
    CHECK(sstable_open(path, &sst) == LSM_OK);
    CHECK(sst != NULL);

    LsmSlice out = {NULL, 0};
    CHECK(sstable_get(sst, lsm_slice_str("anything"), &out) == LSM_ERR_NOT_FOUND);

    sstable_close(sst);
    unlink(path);
}

static void test_sstable_missing_file(void) {
    void *sst = NULL;
    CHECK(sstable_open("/tmp/lsm_test_sstable_does_not_exist.sst", &sst) == LSM_ERR_IO);
    CHECK(sst == NULL);
}

static void test_sstable_null_args(void) {
    CHECK(sstable_build(NULL, NULL, 0) == LSM_ERR_INVALID_ARG);
    CHECK(sstable_open(NULL, NULL) == LSM_ERR_INVALID_ARG);
    CHECK(sstable_get(NULL, lsm_slice_str("k"), NULL) == LSM_ERR_INVALID_ARG);
    sstable_close(NULL);
}

int main(void) {
    test_sstable_roundtrip();
    test_sstable_empty();
    test_sstable_missing_file();
    test_sstable_null_args();
    puts("test_sstable: OK");
    return 0;
}
