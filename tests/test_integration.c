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

typedef struct Wal Wal;
typedef void (*WalRecoverCallback)(uint8_t op, LsmSlice key, LsmSlice value, SequenceNumber seq,
                                   void *user);

typedef struct {
    LsmSlice key;
    LsmSlice value;
} SstableEntry;

extern void *memtable_create(LsmComparator cmp);
extern void memtable_destroy(void *handle);
extern LsmStatus memtable_put(void *handle, LsmInternalKey key, LsmSlice value);
extern LsmStatus memtable_get(void *handle, LsmSlice key, LsmSlice *out_value);

extern LsmStatus wal_open(const char *path, bool sync, Wal **out);
extern LsmStatus wal_append_put(Wal *wal, LsmSlice key, LsmSlice value, SequenceNumber seq);
extern LsmStatus wal_sync(Wal *wal);
extern void wal_close(Wal *wal);
extern LsmStatus wal_recover(Wal *wal, WalRecoverCallback cb, void *user);

extern LsmStatus sstable_build(const char *path, const SstableEntry *entries, size_t count);
extern LsmStatus sstable_open(const char *path, void **out);
extern LsmStatus sstable_get(void *handle, LsmSlice key, LsmSlice *out_value);
extern void sstable_close(void *handle);

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

typedef struct {
    void *memtable;
    int count;
    char keys[32][64];
    char values[32][64];
} ReplayContext;

static void replay_into_memtable(uint8_t op, LsmSlice key, LsmSlice value, SequenceNumber seq,
                                 void *user) {
    ReplayContext *ctx = user;
    if (op != 1)
        return;
    LsmInternalKey ik = {key, seq};
    CHECK(memtable_put(ctx->memtable, ik, value) == LSM_OK);
    if (ctx->count < 32) {
        size_t klen = key.len < 63 ? key.len : 63;
        memcpy(ctx->keys[ctx->count], key.data, klen);
        ctx->keys[ctx->count][klen] = '\0';
        size_t vlen = value.len < 63 ? value.len : 63;
        memcpy(ctx->values[ctx->count], value.data, vlen);
        ctx->values[ctx->count][vlen] = '\0';
        ctx->count++;
    }
}

static void test_wal_to_memtable(void) {
    const char *wal_path = "/tmp/lsm_test_integration.wal";
    unlink(wal_path);

    Wal *wal = NULL;
    CHECK(wal_open(wal_path, false, &wal) == LSM_OK);

    char key_buf[32];
    char val_buf[32];
    for (int i = 0; i < 20; i++) {
        snprintf(key_buf, sizeof(key_buf), "key_%03d", i);
        snprintf(val_buf, sizeof(val_buf), "value_%03d", i);
        CHECK(wal_append_put(wal, lsm_slice_str(key_buf), lsm_slice_str(val_buf),
                             (SequenceNumber)(i + 1)) == LSM_OK);
    }
    CHECK(wal_sync(wal) == LSM_OK);
    wal_close(wal);

    void *mt = memtable_create(bytewise_cmp);
    CHECK(mt != NULL);

    ReplayContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.memtable = mt;

    CHECK(wal_open(wal_path, false, &wal) == LSM_OK);
    CHECK(wal_recover(wal, replay_into_memtable, &ctx) == LSM_OK);
    wal_close(wal);

    CHECK(ctx.count == 20);

    for (int i = 0; i < 20; i++) {
        snprintf(key_buf, sizeof(key_buf), "key_%03d", i);
        LsmSlice out = {NULL, 0};
        CHECK(memtable_get(mt, lsm_slice_str(key_buf), &out) == LSM_OK);
        CHECK(out.len == 9);
    }

    memtable_destroy(mt);
    unlink(wal_path);
}

static void test_memtable_to_sstable(void) {
    const char *sst_path = "/tmp/lsm_test_integration.sst";
    unlink(sst_path);

    SstableEntry entries[10];
    char keys[10][32];
    char vals[10][32];
    for (int i = 0; i < 10; i++) {
        snprintf(keys[i], sizeof(keys[i]), "k%02d", i);
        snprintf(vals[i], sizeof(vals[i]), "v%02d", i);
        entries[i].key = lsm_slice_str(keys[i]);
        entries[i].value = lsm_slice_str(vals[i]);
    }

    CHECK(sstable_build(sst_path, entries, 10) == LSM_OK);

    void *sst = NULL;
    CHECK(sstable_open(sst_path, &sst) == LSM_OK);

    for (int i = 0; i < 10; i++) {
        LsmSlice out = {NULL, 0};
        CHECK(sstable_get(sst, lsm_slice_str(keys[i]), &out) == LSM_OK);
        CHECK(out.len == 3);
        CHECK(memcmp(out.data, vals[i], 3) == 0);
    }

    sstable_close(sst);
    unlink(sst_path);
}

static void test_end_to_end(void) {
    const char *wal_path = "/tmp/lsm_test_e2e.wal";
    const char *sst_path = "/tmp/lsm_test_e2e.sst";
    unlink(wal_path);
    unlink(sst_path);

    Wal *wal = NULL;
    CHECK(wal_open(wal_path, true, &wal) == LSM_OK);

    char key_buf[32];
    char val_buf[32];
    for (int i = 0; i < 15; i++) {
        snprintf(key_buf, sizeof(key_buf), "entity_%02d", i);
        snprintf(val_buf, sizeof(val_buf), "state_%02d", i);
        CHECK(wal_append_put(wal, lsm_slice_str(key_buf), lsm_slice_str(val_buf),
                             (SequenceNumber)(i + 1)) == LSM_OK);
    }
    wal_close(wal);

    void *mt = memtable_create(bytewise_cmp);
    CHECK(mt != NULL);

    ReplayContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.memtable = mt;

    CHECK(wal_open(wal_path, false, &wal) == LSM_OK);
    CHECK(wal_recover(wal, replay_into_memtable, &ctx) == LSM_OK);
    wal_close(wal);
    CHECK(ctx.count == 15);

    SstableEntry entries[32];
    for (int i = 0; i < ctx.count; i++) {
        entries[i].key = lsm_slice_str(ctx.keys[i]);
        entries[i].value = lsm_slice_str(ctx.values[i]);
    }

    CHECK(sstable_build(sst_path, entries, (size_t)ctx.count) == LSM_OK);
    memtable_destroy(mt);

    void *sst = NULL;
    CHECK(sstable_open(sst_path, &sst) == LSM_OK);

    for (int i = 0; i < 15; i++) {
        snprintf(key_buf, sizeof(key_buf), "entity_%02d", i);
        snprintf(val_buf, sizeof(val_buf), "state_%02d", i);
        LsmSlice out = {NULL, 0};
        CHECK(sstable_get(sst, lsm_slice_str(key_buf), &out) == LSM_OK);
        CHECK(out.len == 8);
        CHECK(memcmp(out.data, val_buf, 8) == 0);
    }

    LsmSlice missing = {NULL, 0};
    CHECK(sstable_get(sst, lsm_slice_str("entity_99"), &missing) == LSM_ERR_NOT_FOUND);

    sstable_close(sst);
    unlink(wal_path);
    unlink(sst_path);
}

int main(void) {
    test_wal_to_memtable();
    test_memtable_to_sstable();
    test_end_to_end();
    puts("test_integration: OK");
    return 0;
}
