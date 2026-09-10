#include "lsm_tree.h"
#include "lsm_types.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SST_MAGIC 0x4C534D31u

typedef struct {
    uint32_t data_offset;
    uint32_t data_len;
    uint32_t filter_offset;
    uint32_t filter_len;
    uint32_t index_offset;
    uint32_t index_len;
    uint64_t num_entries;
    uint32_t magic;
} SSTFooter;

typedef struct {
    LsmSlice key;
    LsmSlice value;
} SstableEntry;

typedef struct {
    LsmSlice key;
    uint32_t offset;
    uint32_t len;
} IndexRec;

typedef struct {
    int fd;
    char *path;
    uint32_t data_offset;
    uint32_t data_len;
    uint32_t index_offset;
    uint32_t index_len;
    uint64_t num_entries;
    uint8_t *scratch;
    size_t scratch_cap;
} SSTable;

static LsmStatus write_all(int fd, const void *buf, size_t len) {
    const uint8_t *p = buf;
    size_t off = 0;
    while (off < len) {
        ssize_t w = write(fd, p + off, len - off);
        if (w <= 0)
            return LSM_ERR_IO;
        off += (size_t)w;
    }
    return LSM_OK;
}

static LsmStatus read_all(int fd, void *buf, size_t len) {
    uint8_t *p = buf;
    size_t off = 0;
    while (off < len) {
        ssize_t r = read(fd, p + off, len - off);
        if (r <= 0)
            return LSM_ERR_CORRUPTION;
        off += (size_t)r;
    }
    return LSM_OK;
}

static LsmStatus write_footer(int fd, const SSTFooter *footer) {
    return write_all(fd, footer, sizeof(*footer));
}

static LsmStatus read_footer(int fd, SSTFooter *footer) {
    off_t end = lseek(fd, 0, SEEK_END);
    if (end < (off_t)sizeof(*footer))
        return LSM_ERR_CORRUPTION;
    if (lseek(fd, end - (off_t)sizeof(*footer), SEEK_SET) < 0)
        return LSM_ERR_IO;
    return read_all(fd, footer, sizeof(*footer));
}

LsmStatus sstable_build(const char *path, const SstableEntry *entries, size_t count) {
    if (!path || (!entries && count > 0))
        return LSM_ERR_INVALID_ARG;

    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0)
        return LSM_ERR_IO;

    IndexRec *idx = NULL;
    if (count > 0) {
        idx = malloc(count * sizeof(IndexRec));
        if (!idx) {
            close(fd);
            return LSM_ERR_MEMORY;
        }
    }

    LsmStatus st = LSM_OK;
    uint32_t data_offset = 0;
    uint32_t data_len = 0;

    for (size_t i = 0; i < count; i++) {
        uint32_t klen = (uint32_t)entries[i].key.len;
        uint32_t vlen = (uint32_t)entries[i].value.len;
        uint32_t rec_off = data_offset + data_len;
        uint32_t rec_len = (uint32_t)(2 * sizeof(uint32_t) + klen + vlen);
        if (write_all(fd, &klen, sizeof(klen)) != LSM_OK ||
            write_all(fd, &vlen, sizeof(vlen)) != LSM_OK ||
            write_all(fd, entries[i].key.data, klen) != LSM_OK ||
            write_all(fd, entries[i].value.data, vlen) != LSM_OK) {
            st = LSM_ERR_IO;
            goto done;
        }
        idx[i].key = entries[i].key;
        idx[i].offset = rec_off;
        idx[i].len = rec_len;
        data_len += rec_len;
    }

    uint32_t index_offset = data_offset + data_len;
    uint32_t index_len = 0;

    for (size_t i = 0; i < count; i++) {
        uint32_t klen = (uint32_t)idx[i].key.len;
        if (write_all(fd, &klen, sizeof(klen)) != LSM_OK ||
            write_all(fd, idx[i].key.data, klen) != LSM_OK ||
            write_all(fd, &idx[i].offset, sizeof(idx[i].offset)) != LSM_OK ||
            write_all(fd, &idx[i].len, sizeof(idx[i].len)) != LSM_OK) {
            st = LSM_ERR_IO;
            goto done;
        }
        index_len += (uint32_t)(klen + 3 * sizeof(uint32_t));
    }

    SSTFooter footer;
    footer.data_offset = data_offset;
    footer.data_len = data_len;
    footer.filter_offset = 0;
    footer.filter_len = 0;
    footer.index_offset = index_offset;
    footer.index_len = index_len;
    footer.num_entries = (uint64_t)count;
    footer.magic = SST_MAGIC;

    st = write_footer(fd, &footer);

done:
    free(idx);
    close(fd);
    return st;
}

LsmStatus sstable_open(const char *path, void **out) {
    if (!path || !out)
        return LSM_ERR_INVALID_ARG;

    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return LSM_ERR_IO;

    SSTFooter footer;
    LsmStatus st = read_footer(fd, &footer);
    if (st != LSM_OK) {
        close(fd);
        return st;
    }
    if (footer.magic != SST_MAGIC) {
        close(fd);
        return LSM_ERR_CORRUPTION;
    }

    SSTable *sst = malloc(sizeof(SSTable));
    if (!sst) {
        close(fd);
        return LSM_ERR_MEMORY;
    }
    sst->fd = fd;
    sst->path = strdup(path);
    sst->data_offset = footer.data_offset;
    sst->data_len = footer.data_len;
    sst->index_offset = footer.index_offset;
    sst->index_len = footer.index_len;
    sst->num_entries = footer.num_entries;
    sst->scratch = NULL;
    sst->scratch_cap = 0;

    if (sst->data_len > 0) {
        sst->scratch = malloc(sst->data_len);
        if (!sst->scratch) {
            free(sst->path);
            free(sst);
            close(fd);
            return LSM_ERR_MEMORY;
        }
        if (lseek(fd, (off_t)sst->data_offset, SEEK_SET) < 0) {
            free(sst->scratch);
            free(sst->path);
            free(sst);
            close(fd);
            return LSM_ERR_IO;
        }
        st = read_all(fd, sst->scratch, sst->data_len);
        if (st != LSM_OK) {
            free(sst->scratch);
            free(sst->path);
            free(sst);
            close(fd);
            return st;
        }
        sst->scratch_cap = sst->data_len;
    }

    *out = sst;
    return LSM_OK;
}

LsmStatus sstable_get(void *handle, LsmSlice key, LsmSlice *out_value) {
    SSTable *sst = handle;
    if (!sst || !key.data || !out_value)
        return LSM_ERR_INVALID_ARG;

    size_t off = 0;
    while (off + 2 * sizeof(uint32_t) <= sst->scratch_cap) {
        uint32_t klen;
        uint32_t vlen;
        memcpy(&klen, sst->scratch + off, sizeof(klen));
        off += sizeof(klen);
        memcpy(&vlen, sst->scratch + off, sizeof(vlen));
        off += sizeof(vlen);
        if (off + klen + vlen > sst->scratch_cap)
            return LSM_ERR_CORRUPTION;
        if (klen == key.len && memcmp(sst->scratch + off, key.data, klen) == 0) {
            out_value->data = (const char *)(sst->scratch + off + klen);
            out_value->len = vlen;
            return LSM_OK;
        }
        off += klen + vlen;
    }
    return LSM_ERR_NOT_FOUND;
}

void sstable_close(void *handle) {
    SSTable *sst = handle;
    if (!sst)
        return;
    close(sst->fd);
    free(sst->path);
    free(sst->scratch);
    free(sst);
}
