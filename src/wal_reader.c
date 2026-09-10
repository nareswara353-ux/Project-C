#include "lsm_tree.h"
#include "lsm_types.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum { WAL_OP_PUT = 1, WAL_OP_DELETE = 2 } WalOp;

struct WalReader {
    int fd;
    char *path;
};

static LsmStatus wal_reader_open(const char *path, struct WalReader **out) {
    struct WalReader *r = malloc(sizeof(struct WalReader));
    if (!r)
        return LSM_ERR_MEMORY;
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        free(r);
        return LSM_ERR_IO;
    }
    r->fd = fd;
    r->path = strdup(path);
    *out = r;
    return LSM_OK;
}

static void wal_reader_close(struct WalReader *r) {
    if (!r)
        return;
    close(r->fd);
    free(r->path);
    free(r);
}

typedef void (*WalRecordHandler)(uint8_t op, LsmSlice key, LsmSlice value, uint64_t seq,
                                 void *user);

static LsmStatus wal_reader_read_all(struct WalReader *r, WalRecordHandler handler, void *user) {
    if (!r || !handler)
        return LSM_ERR_INVALID_ARG;
    if (lseek(r->fd, 0, SEEK_SET) < 0)
        return LSM_ERR_IO;
    uint8_t op;
    uint64_t seq;
    uint32_t key_len;
    uint32_t val_len;
    char key_buf[8192];
    char val_buf[8192];
    ssize_t n;
    while ((n = read(r->fd, &op, 1)) == 1) {
        if (read(r->fd, &seq, sizeof(seq)) != sizeof(seq))
            break;
        if (read(r->fd, &key_len, sizeof(key_len)) != sizeof(key_len))
            break;
        if (key_len > sizeof(key_buf))
            break;
        if (read(r->fd, key_buf, key_len) != (ssize_t)key_len)
            break;
        if (read(r->fd, &val_len, sizeof(val_len)) != sizeof(val_len))
            break;
        if (val_len > sizeof(val_buf))
            break;
        if (read(r->fd, val_buf, val_len) != (ssize_t)val_len)
            break;
        LsmSlice key = {key_buf, key_len};
        LsmSlice val = {val_buf, val_len};
        handler(op, key, val, seq, user);
    }
    return LSM_OK;
}

static void replay_handler(uint8_t op, LsmSlice key, LsmSlice value, uint64_t seq, void *user) {
    void *memtable = user;
    if (!memtable)
        return;
    LsmInternalKey ik = {key, seq};
    extern LsmStatus memtable_put(void *handle, LsmInternalKey key, LsmSlice value);
    if (op == WAL_OP_PUT) {
        memtable_put(memtable, ik, value);
    } else if (op == WAL_OP_DELETE) {
        LsmSlice empty = {NULL, 0};
        memtable_put(memtable, ik, empty);
    }
}

LsmStatus wal_replay_into_memtable(const char *wal_path, void *memtable) {
    if (!wal_path || !memtable)
        return LSM_ERR_INVALID_ARG;
    struct WalReader *r = NULL;
    LsmStatus st = wal_reader_open(wal_path, &r);
    if (st != LSM_OK)
        return st;
    st = wal_reader_read_all(r, replay_handler, memtable);
    wal_reader_close(r);
    return st;
}
