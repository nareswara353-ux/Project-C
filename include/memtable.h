#ifndef LSM_MEMTABLE_H
#define LSM_MEMTABLE_H

#include "lsm_types.h"

void *memtable_create(LsmComparator cmp);
void memtable_destroy(void *handle);
LsmStatus memtable_put(void *handle, LsmInternalKey key, LsmSlice value);
LsmStatus memtable_get(void *handle, LsmSlice key, LsmSlice *out_value);
size_t memtable_size(void *handle);
void *memtable_iterator(void *handle);
void memtable_iterator_next(void *iter);
LsmSlice memtable_iterator_key(void *iter);
LsmSlice memtable_iterator_value(void *iter);
bool memtable_iterator_valid(void *iter);

#endif
