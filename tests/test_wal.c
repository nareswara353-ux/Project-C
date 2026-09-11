#include "lsm_tree.h"
#include "lsm_types.h"

#include <fcntl.h>
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

extern LsmStatus wal_open(const char *path, bool sync, Wal **out);
extern LsmStatus wal_append_put(Wal *wal, LsmSlice key, LsmSlice value, SequenceNumber seq);
extern LsmStatus wal_append_delete(Wal *wal, LsmSlice key, SequenceNumber seq);
extern LsmStatus wal_sync(Wal *wal);
extern void wal_close(Wal *wal);
extern LsmStatus wal_recover(Wal *wal, WalRecoverCallback cb, void *user);

typedef struct {
    int count;
    uint8_t ops[16];
    char keys[16][32];
    size_t key_lens[16];
    char values[16][32];
    size_t value_lens[16];
    uint64_t seqs[16];
} ReplayLog;

static void capture(uint8_t op, LsmSlice key, LsmSlice value, SequenceNumber seq, void *user) {
    ReplayLog *log = user;
    if (log->count >= 16)
        return;
    int i = log->count;
    log->ops[i] = op;
    size_t klen = key.len < 32 ? key.len : 31;
    memcpy(log->keys[i], key.data, klen);
    log->keys[i][klen] = '\0';
    log->key_lens[i] = klen;
    size_t vlen = value.len < 32 ? value.len : 31;
    memcpy(log->values[i], value.data, vlen);
    log->values[i][vlen] = '\0';
    log->value_lens[i] = vlen;
    log->seqs[i] = seq;
    log->count++;
}

static void test_wal_roundtrip(void) {
    const char *path = "/tmp/lsm_test_wal_roundtrip.log";
    unlink(path);

    Wal *wal = NULL;
    CHECK(wal_open(path, false, &wal) == LSM_OK);
    CHECK(wal != NULL);

    CHECK(wal_append_put(wal, lsm_slice_str("alpha"), lsm_slice_str("one"), 1) == LSM_OK);
    CHECK(wal_append_put(wal, lsm_slice_str("beta"), lsm_slice_str("two"), 2) == LSM_OK);
    CHECK(wal_append_delete(wal, lsm_slice_str("alpha"), 3) == LSM_OK);
    CHECK(wal_sync(wal) == LSM_OK);
    wal_close(wal);

    CHECK(wal_open(path, false, &wal) == LSM_OK);
    ReplayLog log;
    memset(&log, 0, sizeof(log));
    CHECK(wal_recover(wal, capture, &log) == LSM_OK);
    wal_close(wal);

    CHECK(log.count == 3);
    CHECK(log.ops[0] == 1);
    CHECK(strcmp(log.keys[0], "alpha") == 0);
    CHECK(strcmp(log.values[0], "one") == 0);
    CHECK(log.seqs[0] == 1);

    CHECK(log.ops[1] == 1);
    CHECK(strcmp(log.keys[1], "beta") == 0);
    CHECK(strcmp(log.values[1], "two") == 0);
    CHECK(log.seqs[1] == 2);

    CHECK(log.ops[2] == 2);
    CHECK(strcmp(log.keys[2], "alpha") == 0);
    CHECK(log.seqs[2] == 3);

    unlink(path);
}

static void test_wal_sync_flag(void) {
    const char *path = "/tmp/lsm_test_wal_sync.log";
    unlink(path);

    Wal *wal = NULL;
    CHECK(wal_open(path, true, &wal) == LSM_OK);
    CHECK(wal_append_put(wal, lsm_slice_str("k"), lsm_slice_str("v"), 42) == LSM_OK);
    wal_close(wal);

    int fd = open(path, O_RDONLY);
    CHECK(fd >= 0);
    off_t size = lseek(fd, 0, SEEK_END);
    CHECK(size > 0);
    close(fd);

    unlink(path);
}

static void test_wal_empty_recover(void) {
    const char *path = "/tmp/lsm_test_wal_empty.log";
    unlink(path);

    Wal *wal = NULL;
    CHECK(wal_open(path, false, &wal) == LSM_OK);
    wal_close(wal);

    CHECK(wal_open(path, false, &wal) == LSM_OK);
    ReplayLog log;
    memset(&log, 0, sizeof(log));
    CHECK(wal_recover(wal, capture, &log) == LSM_OK);
    CHECK(log.count == 0);
    wal_close(wal);

    unlink(path);
}

static void test_wal_null_args(void) {
    CHECK(wal_open(NULL, false, NULL) == LSM_ERR_INVALID_ARG);
    CHECK(wal_sync(NULL) == LSM_ERR_INVALID_ARG);
    CHECK(wal_recover(NULL, NULL, NULL) == LSM_ERR_INVALID_ARG);
}

int main(void) {
    test_wal_roundtrip();
    test_wal_sync_flag();
    test_wal_empty_recover();
    test_wal_null_args();
    puts("test_wal: OK");
    return 0;
}
