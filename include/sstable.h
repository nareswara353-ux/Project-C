#ifndef LSM_SSTABLE_H
#define LSM_SSTABLE_H

#include "lsm_types.h"

typedef struct {
    LsmSlice key;
    LsmSlice value;
} SstableEntry;

LsmStatus sstable_build(const char *path, const SstableEntry *entries, size_t count);
LsmStatus sstable_open(const char *path, void **out);
LsmStatus sstable_get(void *handle, LsmSlice key, LsmSlice *out_value);
void sstable_close(void *handle);

#endif
