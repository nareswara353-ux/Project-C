#include "lsm_tree.h"
#include "lsm_types.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum { WAL_OP_PUT = 1, WAL_OP_DELETE = 2 } WalOp;

typedef struct Wal {
    int fd;
    char *path;
    bool sync;
    size_t offset;
} Wal;

static LsmStatus wal_open_fd(const char *path, bool sync, Wal **out) {
    Wal *w = malloc(sizeof(Wal));
    if (!w)
        return LSM_ERR_MEMORY;
    int fd = open(path, O_CREAT | O_RDWR | O_APPEND, 0644);
    if (fd < 0) {
        free(w);
        return LSM_ERR_IO;
    }
    w->fd = fd;
    w->path = strdup(path);
    w->sync = sync;
    w->offset = (size_t)lseek(fd, 0, SEEK_END);
    *out = w;
    return LSM_OK;
}

LsmStatus wal_open(const char *path, bool sync, Wal **out) {
    if (!path || !out)
        return LSM_ERR_INVALID_ARG;
    return wal_open_fd(path, sync, out);
}

static LsmStatus wal_append_record(Wal *wal, uint8_t op, LsmSlice key, LsmSlice value,
                                   SequenceNumber seq) {
    if (!wal || !key.data)
        return LSM_ERR_INVALID_ARG;
    uint32_t key_len = (uint32_t)key.len;
    uint32_t val_len = (uint32_t)value.len;
    uint64_t seq_be = seq;
    uint32_t key_len_be = key_len;
    uint32_t val_len_be = val_len;
    ssize_t written = 0;
    written += write(wal->fd, &op, 1);
    written += write(wal->fd, &seq_be, sizeof(seq_be));
    written += write(wal->fd, &key_len_be, sizeof(key_len_be));
    written += write(wal->fd, key.data, key_len);
    written += write(wal->fd, &val_len_be, sizeof(val_len_be));
    written += write(wal->fd, value.data, val_len);
    if (written < 0)
        return LSM_ERR_IO;
    wal->offset += (size_t)written;
    if (wal->sync) {
        if (fsync(wal->fd) < 0)
            return LSM_ERR_IO;
    }
    return LSM_OK;
}

LsmStatus wal_append_put(Wal *wal, LsmSlice key, LsmSlice value, SequenceNumber seq) {
    return wal_append_record(wal, WAL_OP_PUT, key, value, seq);
}

LsmStatus wal_append_delete(Wal *wal, LsmSlice key, SequenceNumber seq) {
    LsmSlice empty = {NULL, 0};
    return wal_append_record(wal, WAL_OP_DELETE, key, empty, seq);
}

LsmStatus wal_sync(Wal *wal) {
    if (!wal)
        return LSM_ERR_INVALID_ARG;
    if (fsync(wal->fd) < 0)
        return LSM_ERR_IO;
    return LSM_OK;
}

void wal_close(Wal *wal) {
    if (!wal)
        return;
    close(wal->fd);
    free(wal->path);
    free(wal);
}

typedef void (*WalRecoverCallback)(uint8_t op, LsmSlice key, LsmSlice value, SequenceNumber seq,
                                   void *user);

LsmStatus wal_recover(Wal *wal, WalRecoverCallback cb, void *user) {
    if (!wal || !cb)
        return LSM_ERR_INVALID_ARG;
    if (lseek(wal->fd, 0, SEEK_SET) < 0)
        return LSM_ERR_IO;
    uint8_t op;
    uint64_t seq;
    uint32_t key_len;
    uint32_t val_len;
    char key_buf[4096];
    char val_buf[4096];
    ssize_t n;
    while ((n = read(wal->fd, &op, 1)) == 1) {
        if (read(wal->fd, &seq, sizeof(seq)) != sizeof(seq))
            break;
        if (read(wal->fd, &key_len, sizeof(key_len)) != sizeof(key_len))
            break;
        if (key_len > sizeof(key_buf))
            break;
        if (read(wal->fd, key_buf, key_len) != (ssize_t)key_len)
            break;
        if (read(wal->fd, &val_len, sizeof(val_len)) != sizeof(val_len))
            break;
        if (val_len > sizeof(val_buf))
            break;
        if (read(wal->fd, val_buf, val_len) != (ssize_t)val_len)
            break;
        LsmSlice key = {key_buf, key_len};
        LsmSlice val = {val_buf, val_len};
        cb(op, key, val, seq, user);
    }
    return LSM_OK;
}
