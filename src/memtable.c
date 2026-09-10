#include "lsm_tree.h"
#include "lsm_types.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

typedef struct MemTableEntry {
    LsmInternalKey key;
    LsmSlice value;
    struct MemTableEntry *next[1];
} MemTableEntry;

typedef struct MemTable {
    atomic_size_t size_bytes;
    int max_level;
    MemTableEntry *head;
    LsmComparator cmp;
} MemTable;

static MemTableEntry *entry_create(LsmInternalKey key, LsmSlice value, int level) {
    MemTableEntry *e = malloc(sizeof(MemTableEntry) + level * sizeof(MemTableEntry *));
    if (!e)
        return NULL;
    e->key = key;
    e->value = value;
    for (int i = 0; i <= level; i++)
        e->next[i] = NULL;
    return e;
}

static void entry_destroy(MemTableEntry *e) {
    free(e);
}

void *memtable_create(LsmComparator cmp) {
    MemTable *mt = malloc(sizeof(MemTable));
    if (!mt)
        return NULL;
    mt->max_level = 16;
    LsmInternalKey zero_key = {{NULL, 0}, 0};
    LsmSlice zero_value = {NULL, 0};
    mt->head = entry_create(zero_key, zero_value, mt->max_level);
    if (!mt->head) {
        free(mt);
        return NULL;
    }
    mt->cmp = cmp;
    atomic_init(&mt->size_bytes,
                sizeof(MemTable) + sizeof(MemTableEntry) * (size_t)(mt->max_level + 1));
    return mt;
}

void memtable_destroy(void *handle) {
    MemTable *mt = handle;
    if (!mt)
        return;
    MemTableEntry *cur = mt->head;
    while (cur) {
        MemTableEntry *next = cur->next[0];
        entry_destroy(cur);
        cur = next;
    }
    free(mt);
}

LsmStatus memtable_put(void *handle, LsmInternalKey key, LsmSlice value) {
    MemTable *mt = handle;
    if (!mt)
        return LSM_ERR_INVALID_ARG;
    int level = mt->max_level;
    MemTableEntry *e = entry_create(key, value, level);
    if (!e)
        return LSM_ERR_MEMORY;
    MemTableEntry *cur = mt->head;
    MemTableEntry **update = malloc((size_t)(mt->max_level + 1) * sizeof(MemTableEntry *));
    if (!update) {
        entry_destroy(e);
        return LSM_ERR_MEMORY;
    }
    for (int i = mt->max_level; i >= 0; i--) {
        while (cur->next[i] &&
               mt->cmp(cur->next[i]->key.user_key.data, cur->next[i]->key.user_key.len,
                       key.user_key.data, key.user_key.len) < 0) {
            cur = cur->next[i];
        }
        update[i] = cur;
    }
    cur = cur->next[0];
    for (int i = 0; i <= level; i++) {
        e->next[i] = update[i]->next[i];
        update[i]->next[i] = e;
    }
    free(update);
    atomic_fetch_add(&mt->size_bytes, sizeof(MemTableEntry) +
                                          (size_t)level * sizeof(MemTableEntry *) +
                                          key.user_key.len + value.len);
    return LSM_OK;
}

LsmStatus memtable_get(void *handle, LsmSlice key, LsmSlice *out_value) {
    MemTable *mt = handle;
    if (!mt || !out_value)
        return LSM_ERR_INVALID_ARG;
    MemTableEntry *cur = mt->head->next[0];
    while (cur) {
        int cmp = mt->cmp(cur->key.user_key.data, cur->key.user_key.len, key.data, key.len);
        if (cmp == 0) {
            *out_value = cur->value;
            return LSM_OK;
        }
        cur = cur->next[0];
    }
    return LSM_ERR_NOT_FOUND;
}

size_t memtable_size(void *handle) {
    MemTable *mt = handle;
    if (!mt)
        return 0;
    return atomic_load(&mt->size_bytes);
}

void *memtable_iterator(void *handle) {
    MemTable *mt = handle;
    if (!mt)
        return NULL;
    return mt->head->next[0];
}

void memtable_iterator_next(void *iter) {
    MemTableEntry *e = iter;
    if (e)
        e = e->next[0];
}

LsmSlice memtable_iterator_key(void *iter) {
    MemTableEntry *e = iter;
    if (!e)
        return (LsmSlice){NULL, 0};
    return lsm_internal_key_slice(&e->key);
}

LsmSlice memtable_iterator_value(void *iter) {
    MemTableEntry *e = iter;
    if (!e)
        return (LsmSlice){NULL, 0};
    return e->value;
}

bool memtable_iterator_valid(void *iter) {
    return iter != NULL;
}
