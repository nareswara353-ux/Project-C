#include "lsm_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void trim(char *str) {
    char *end;
    while (isspace((unsigned char)*str))
        str++;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;
    *(end + 1) = '\0';
}

static void parse_key_value(char *line, char **key, char **value) {
    *key = NULL;
    *value = NULL;
    char *eq = strchr(line, '=');
    if (!eq)
        return;
    *key = line;
    *eq = '\0';
    *value = eq + 1;
    trim(*key);
    trim(*value);
}

static void apply_option(LsmOptions *opts, const char *key, const char *value) {
    if (strcmp(key, "db_path") == 0) {
        opts->db_path = strdup(value);
    } else if (strcmp(key, "memtable_size_mb") == 0) {
        opts->memtable_size_mb = (size_t)atoi(value);
    } else if (strcmp(key, "max_open_files") == 0) {
        opts->max_open_files = (size_t)atoi(value);
    } else if (strcmp(key, "enable_wal") == 0) {
        opts->enable_wal = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
    } else if (strcmp(key, "sync_wal") == 0) {
        opts->sync_wal = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
    } else if (strcmp(key, "block_size_kb") == 0) {
        opts->block_size_kb = (size_t)atoi(value);
    } else if (strcmp(key, "bloom_bits_per_key") == 0) {
        opts->bloom_bits_per_key = (size_t)atoi(value);
    } else if (strcmp(key, "compaction_interval_sec") == 0) {
        opts->compaction_interval_sec = (size_t)atoi(value);
    }
}

LsmOptions lsm_config_from_env(void) {
    LsmOptions opts = lsm_options_default();
    const char *val;
    if ((val = getenv("LSM_DB_PATH")))
        opts.db_path = val;
    if ((val = getenv("LSM_MEMTABLE_SIZE_MB")))
        opts.memtable_size_mb = (size_t)atoi(val);
    if ((val = getenv("LSM_MAX_OPEN_FILES")))
        opts.max_open_files = (size_t)atoi(val);
    if ((val = getenv("LSM_ENABLE_WAL")))
        opts.enable_wal = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
    if ((val = getenv("LSM_SYNC_WAL")))
        opts.sync_wal = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
    if ((val = getenv("LSM_BLOCK_SIZE_KB")))
        opts.block_size_kb = (size_t)atoi(val);
    if ((val = getenv("LSM_BLOOM_BITS")))
        opts.bloom_bits_per_key = (size_t)atoi(val);
    if ((val = getenv("LSM_COMPACTION_INTERVAL")))
        opts.compaction_interval_sec = (size_t)atoi(val);
    return opts;
}

LsmOptions lsm_config_from_file(const char *path) {
    LsmOptions opts = lsm_options_default();
    FILE *f = fopen(path, "r");
    if (!f)
        return opts;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *key = NULL;
        char *value = NULL;
        parse_key_value(line, &key, &value);
        if (key && value)
            apply_option(&opts, key, value);
    }
    fclose(f);
    return opts;
}

LsmStatus lsm_config_validate(const LsmOptions *opts) {
    if (!opts->db_path || strlen(opts->db_path) == 0)
        return LSM_ERR_INVALID_ARG;
    if (opts->memtable_size_mb < 1 || opts->memtable_size_mb > 1024)
        return LSM_ERR_INVALID_ARG;
    if (opts->max_open_files < 10 || opts->max_open_files > 65536)
        return LSM_ERR_INVALID_ARG;
    if (opts->block_size_kb < 1 || opts->block_size_kb > 65536)
        return LSM_ERR_INVALID_ARG;
    if (opts->bloom_bits_per_key < 1 || opts->bloom_bits_per_key > 64)
        return LSM_ERR_INVALID_ARG;
    if (opts->compaction_interval_sec > 86400)
        return LSM_ERR_INVALID_ARG;
    return LSM_OK;
}
