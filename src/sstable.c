#include "lsm_types.h"
#include "lsm_tree.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

typedef struct {
    uint32_t crc;
    uint32_t data_offset;
    uint32_t data_len;
    uint32_t filter_offset;
    uint32_t filter_len;
    uint32_t index_offset;
    uint32_t index_len;
    uint64_t num_entries;
} SSTFooter;

typedef struct {
    LsmSlice key;
    uint32_t offset;
    uint32_t len;
} IndexEntry;

typedef struct {
    int fd;
    char* path;
    uint32_t block_size;
    uint32_t data_offset;
    uint32_t data_len;
    uint32_t filter_offset;
    uint32_t filter_len;
    uint32_t index_offset;
    uint32_t index_len;
    uint64_t num_entries;
} SSTable;

static uint32_t crc32(const void* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* bytes = data;
    for (size_t i = 0; i < len; i++) {
        crc ^= bytes[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else crc >>= 1;
        }
    }
    return ~crc;
}

static LsmStatus write_footer(int fd, const SSTFooter* footer) {
    if (lseek(fd, 0, SEEK_END) < 0) return LSM_ERR_IO;
    ssize_t w = write(fd, footer, sizeof(SSTFooter));
    if (w != sizeof(SSTFooter)) return LSM_ERR_IO;
    return LSM_OK;
}

static LsmStatus read_footer(int fd, SSTFooter* footer) {
    off_t end = lseek(fd, 0, SEEK_END);
    if (end < (off_t)sizeof(SSTFooter)) return LSM_ERR_CORRUPTION;
    if (lseek(fd, end - sizeof(SSTFooter), SEEK_SET) < 0) return LSM_ERR_IO;
    ssize_t r = read(fd, footer, sizeof(SSTFooter));
    if (r != sizeof(SSTFooter)) return LSM_ERR_CORRUPTION;
    return LSM_OK;
}

LsmStatus sstable_build(const char* path, void* iter, uint32_t block_size, uint32_t bits_per_key) {
    (void)bits_per_key;
    if (!path || !iter) return LSM_ERR_INVALID_ARG;
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) return LSM_ERR_IO;
    uint32_t data_off = 0;
    uint32_t data_len = 0;
    uint32_t index_off = 0;
    uint32_t index_len = 0;
    uint32_t filter_off = 0;
    uint32_t filter_len = 0;
    uint64_t num_entries = 0;
    char block[block_size];
    uint32_t block_used = 0;
    char index_buf[block_size * 2];
    uint32_t index_used = 0;
    while (1) {
        LsmSlice key, value;
        if (!iter) break;
        if (block_used + key.len + value.len + 8 > block_size) {
            if (block_used > 0) {
                ssize_t w = write(fd, block, block_used);
                if (w != (ssize_t)block_used) { close(fd); return LSM_ERR_IO; }
                data_len += block_used;
                block_used = 0;
            }
        }
        break;
    }
    close(fd);
    return LSM_OK;
}

LsmStatus sstable_open(const char* path, void** out) {
    if (!path || !out) return LSM_ERR_INVALID_ARG;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return LSM_ERR_IO;
    SSTFooter footer;
    LsmStatus st = read_footer(fd, &footer);
    if (st != LSM_OK) { close(fd); return st; }
    SSTable* sst = malloc(sizeof(SSTable));
    if (!sst) { close(fd); return LSM_ERR_MEMORY; }
    sst->fd = fd;
    sst->path = strdup(path);
    sst->block_size = 4096;
    sst->data_offset = footer.data_offset;
    sst->data_len = footer.data_len;
    sst->filter_offset = footer.filter_offset;
    sst->filter_len = footer.filter_len;
    sst->index_offset = footer.index_offset;
    sst->index_len = footer.index_len;
    sst->num_entries = footer.num_entries;
    *out = sst;
    return LSM_OK;
}

LsmStatus sstable_get(void* handle, LsmSlice key, LsmSlice* out_value) {
    (void)handle;
    (void)key;
    (void)out_value;
    return LSM_ERR_NOT_FOUND;
}

void sstable_close(void* handle) {
    SSTable* sst = handle;
    if (!sst) return;
    close(sst->fd);
    free(sst->path);
    free(sst);
}
