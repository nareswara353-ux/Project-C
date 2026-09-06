#ifndef LSM_TREE_H
#define LSM_TREE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct LsmTree LsmTree;
typedef struct LsmIterator LsmIterator;
typedef struct LsmOptions LsmOptions;

typedef enum {
    LSM_OK = 0,
    LSM_ERR_IO,
    LSM_ERR_CORRUPTION,
    LSM_ERR_NOT_FOUND,
    LSM_ERR_FULL,
    LSM_ERR_INVALID_ARG,
    LSM_ERR_MEMORY,
    LSM_ERR_BUSY
} LsmStatus;

typedef struct {
    const char* data;
    size_t len;
} LsmSlice;

struct LsmOptions {
    const char* db_path;
    size_t memtable_size_mb;
    size_t max_open_files;
    bool enable_wal;
    bool sync_wal;
    size_t block_size_kb;
    size_t bloom_bits_per_key;
    size_t compaction_interval_sec;
};

LsmStatus lsm_open(const LsmOptions* options, LsmTree** out);
LsmStatus lsm_put(LsmTree* tree, LsmSlice key, LsmSlice value);
LsmStatus lsm_get(LsmTree* tree, LsmSlice key, LsmSlice* out_value);
LsmStatus lsm_delete(LsmTree* tree, LsmSlice key);
LsmStatus lsm_iterator(LsmTree* tree, LsmIterator** out);
bool lsm_iterator_valid(const LsmIterator* it);
void lsm_iterator_next(LsmIterator* it);
LsmSlice lsm_iterator_key(const LsmIterator* it);
LsmSlice lsm_iterator_value(const LsmIterator* it);
void lsm_iterator_destroy(LsmIterator* it);
void lsm_close(LsmTree* tree);
LsmOptions lsm_options_default(void);

#endif
