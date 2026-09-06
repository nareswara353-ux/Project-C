#include "lsm_types.h"
#include "lsm_tree.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct {
    LsmSlice key;
    uint32_t offset;
    uint32_t len;
} IndexEntry;

typedef struct {
    IndexEntry* entries;
    size_t count;
    size_t capacity;
} IndexBuilder;

IndexBuilder* index_builder_create(void) {
    IndexBuilder* b = malloc(sizeof(IndexBuilder));
    if (!b) return NULL;
    b->entries = NULL;
    b->count = 0;
    b->capacity = 0;
    return b;
}

void index_builder_destroy(IndexBuilder* b) {
    if (!b) return;
    for (size_t i = 0; i < b->count; i++) {
        free((void*)b->entries[i].key.data);
    }
    free(b->entries);
    free(b);
}

LsmStatus index_builder_add(IndexBuilder* b, LsmSlice key, uint32_t offset, uint32_t len) {
    if (!b || !key.data) return LSM_ERR_INVALID_ARG;
    if (b->count >= b->capacity) {
        size_t new_cap = b->capacity ? b->capacity * 2 : 16;
        IndexEntry* new_entries = realloc(b->entries, new_cap * sizeof(IndexEntry));
        if (!new_entries) return LSM_ERR_MEMORY;
        b->entries = new_entries;
        b->capacity = new_cap;
    }
    char* key_copy = malloc(key.len);
    if (!key_copy) return LSM_ERR_MEMORY;
    memcpy(key_copy, key.data, key.len);
    b->entries[b->count].key = lsm_slice(key_copy, key.len);
    b->entries[b->count].offset = offset;
    b->entries[b->count].len = len;
    b->count++;
    return LSM_OK;
}

static int entry_cmp(const void* a, const void* b) {
    const IndexEntry* ea = a;
    const IndexEntry* eb = b;
    return lsm_slice_cmp(ea->key, eb->key);
}

void index_builder_sort(IndexBuilder* b) {
    if (!b || b->count < 2) return;
    qsort(b->entries, b->count, sizeof(IndexEntry), entry_cmp);
}

size_t index_builder_serialize(IndexBuilder* b, uint8_t* out, size_t out_len) {
    if (!b || !out) return 0;
    size_t needed = 0;
    for (size_t i = 0; i < b->count; i++) {
        needed += sizeof(uint32_t) + b->entries[i].key.len + sizeof(uint32_t) + sizeof(uint32_t);
    }
    if (out_len < needed) return 0;
    uint8_t* ptr = out;
    for (size_t i = 0; i < b->count; i++) {
        uint32_t key_len = (uint32_t)b->entries[i].key.len;
        memcpy(ptr, &key_len, sizeof(key_len));
        ptr += sizeof(key_len);
        memcpy(ptr, b->entries[i].key.data, key_len);
        ptr += key_len;
        memcpy(ptr, &b->entries[i].offset, sizeof(b->entries[i].offset));
        ptr += sizeof(b->entries[i].offset);
        memcpy(ptr, &b->entries[i].len, sizeof(b->entries[i].len));
        ptr += sizeof(b->entries[i].len);
    }
    return needed;
}

IndexBuilder* index_builder_deserialize(const uint8_t* data, size_t len) {
    if (!data || len == 0) return NULL;
    IndexBuilder* b = index_builder_create();
    if (!b) return NULL;
    const uint8_t* ptr = data;
    const uint8_t* end = data + len;
    while (ptr < end) {
        uint32_t key_len;
        if (ptr + sizeof(key_len) > end) goto error;
        memcpy(&key_len, ptr, sizeof(key_len));
        ptr += sizeof(key_len);
        if (ptr + key_len > end) goto error;
        LsmSlice key = { (const char*)ptr, key_len };
        ptr += key_len;
        uint32_t offset, block_len;
        if (ptr + sizeof(offset) > end) goto error;
        memcpy(&offset, ptr, sizeof(offset));
        ptr += sizeof(offset);
        if (ptr + sizeof(block_len) > end) goto error;
        memcpy(&block_len, ptr, sizeof(block_len));
        ptr += sizeof(block_len);
        LsmStatus st = index_builder_add(b, key, offset, block_len);
        if (st != LSM_OK) goto error;
    }
    return b;
error:
    index_builder_destroy(b);
    return NULL;
}

IndexEntry* index_builder_find(IndexBuilder* b, LsmSlice key) {
    if (!b || !key.data) return NULL;
    size_t lo = 0, hi = b->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int cmp = lsm_slice_cmp(b->entries[mid].key, key);
        if (cmp == 0) return &b->entries[mid];
        if (cmp < 0) lo = mid + 1;
        else hi = mid;
    }
    if (lo > 0) return &b->entries[lo - 1];
    return NULL;
}
