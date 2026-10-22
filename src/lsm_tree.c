#include "lsm_tree.h"

#include "lsm_iterator.h"
#include "lsm_types.h"

#include <stdlib.h>
#include <string.h>

struct LsmTree {
    LsmOptions opts;
    void *memtable;
    void *wal;
    SequenceNumber seq;
};

LsmOptions lsm_options_default(void) {
    LsmOptions opts;
    opts.db_path = "/tmp/lsm_db";
    opts.memtable_size_mb = 4;
    opts.max_open_files = 1000;
    opts.enable_wal = true;
    opts.sync_wal = false;
    opts.block_size_kb = 4;
    opts.bloom_bits_per_key = 10;
    opts.compaction_interval_sec = 60;
    opts.owns_db_path = false;
    return opts;
}

LsmStatus lsm_open(const LsmOptions *options, LsmTree **out) {
    if (!options || !out)
        return LSM_ERR_INVALID_ARG;
    LsmTree *tree = malloc(sizeof(LsmTree));
    if (!tree)
        return LSM_ERR_MEMORY;
    tree->opts = *options;
    tree->memtable = NULL;
    tree->wal = NULL;
    tree->seq = 0;
    *out = tree;
    return LSM_OK;
}

LsmStatus lsm_put(LsmTree *tree, LsmSlice key, LsmSlice value) {
    if (!tree || !key.data || !value.data)
        return LSM_ERR_INVALID_ARG;
    tree->seq++;
    return LSM_OK;
}

LsmStatus lsm_get(LsmTree *tree, LsmSlice key, LsmSlice *out_value) {
    if (!tree || !key.data || !out_value)
        return LSM_ERR_INVALID_ARG;
    return LSM_ERR_NOT_FOUND;
}

LsmStatus lsm_delete(LsmTree *tree, LsmSlice key) {
    if (!tree || !key.data)
        return LSM_ERR_INVALID_ARG;
    tree->seq++;
    return LSM_OK;
}

LsmStatus lsm_iterator(LsmTree *tree, LsmIterator **out) {
    if (!tree || !out)
        return LSM_ERR_INVALID_ARG;
    *out = lsm_iterator_from_impl(NULL, NULL);
    return *out ? LSM_OK : LSM_ERR_MEMORY;
}

void lsm_close(LsmTree *tree) {
    if (!tree)
        return;
    if (tree->opts.owns_db_path && tree->opts.db_path)
        free((void *)tree->opts.db_path);
    free(tree);
}
