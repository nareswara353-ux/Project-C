#include "lsm_tree.h"
#include "lsm_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void check_impl(int ok, const char *expr, const char *file, int line) {
    if (!ok) {
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", file, line, expr);
        exit(1);
    }
}

#define CHECK(cond) check_impl((cond) ? 1 : 0, #cond, __FILE__, __LINE__)

extern LsmOptions lsm_config_from_env(void);
extern LsmOptions lsm_config_from_file(const char *path);
extern LsmStatus lsm_config_validate(const LsmOptions *opts);

static void clear_env(void) {
    unsetenv("LSM_DB_PATH");
    unsetenv("LSM_MEMTABLE_SIZE_MB");
    unsetenv("LSM_MAX_OPEN_FILES");
    unsetenv("LSM_ENABLE_WAL");
    unsetenv("LSM_SYNC_WAL");
    unsetenv("LSM_BLOCK_SIZE_KB");
    unsetenv("LSM_BLOOM_BITS");
    unsetenv("LSM_COMPACTION_INTERVAL");
}

static void test_defaults(void) {
    LsmOptions o = lsm_options_default();
    CHECK(o.db_path != NULL);
    CHECK(strcmp(o.db_path, "/tmp/lsm_db") == 0);
    CHECK(o.memtable_size_mb == 4);
    CHECK(o.max_open_files == 1000);
    CHECK(o.enable_wal == true);
    CHECK(o.sync_wal == false);
    CHECK(o.block_size_kb == 4);
    CHECK(o.bloom_bits_per_key == 10);
    CHECK(o.compaction_interval_sec == 60);
    CHECK(o.owns_db_path == false);
    CHECK(lsm_config_validate(&o) == LSM_OK);
    lsm_options_destroy(&o);
}

static void test_env_override(void) {
    clear_env();
    setenv("LSM_DB_PATH", "/tmp/lsm_env_path", 1);
    setenv("LSM_MEMTABLE_SIZE_MB", "64", 1);
    setenv("LSM_MAX_OPEN_FILES", "512", 1);
    setenv("LSM_ENABLE_WAL", "true", 1);
    setenv("LSM_SYNC_WAL", "1", 1);
    setenv("LSM_BLOCK_SIZE_KB", "8", 1);
    setenv("LSM_BLOOM_BITS", "12", 1);
    setenv("LSM_COMPACTION_INTERVAL", "120", 1);

    LsmOptions o = lsm_config_from_env();
    CHECK(o.db_path != NULL);
    CHECK(strcmp(o.db_path, "/tmp/lsm_env_path") == 0);
    CHECK(o.owns_db_path == true);
    CHECK(o.memtable_size_mb == 64);
    CHECK(o.max_open_files == 512);
    CHECK(o.enable_wal == true);
    CHECK(o.sync_wal == true);
    CHECK(o.block_size_kb == 8);
    CHECK(o.bloom_bits_per_key == 12);
    CHECK(o.compaction_interval_sec == 120);
    CHECK(lsm_config_validate(&o) == LSM_OK);
    lsm_options_destroy(&o);

    clear_env();
}

static void test_env_falsy(void) {
    clear_env();
    setenv("LSM_ENABLE_WAL", "false", 1);
    setenv("LSM_SYNC_WAL", "0", 1);

    LsmOptions o = lsm_config_from_env();
    CHECK(o.enable_wal == false);
    CHECK(o.sync_wal == false);
    lsm_options_destroy(&o);

    clear_env();
}

static void test_file_override(void) {
    const char *path = "/tmp/lsm_test_config.cfg";
    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    fprintf(f, "db_path = /tmp/lsm_file_path\n");
    fprintf(f, "memtable_size_mb = 32\n");
    fprintf(f, "max_open_files = 256\n");
    fprintf(f, "enable_wal = true\n");
    fprintf(f, "sync_wal = 1\n");
    fprintf(f, "block_size_kb = 16\n");
    fprintf(f, "bloom_bits_per_key = 8\n");
    fprintf(f, "compaction_interval_sec = 300\n");
    fclose(f);

    LsmOptions o = lsm_config_from_file(path);
    CHECK(o.db_path != NULL);
    CHECK(strcmp(o.db_path, "/tmp/lsm_file_path") == 0);
    CHECK(o.owns_db_path == true);
    CHECK(o.memtable_size_mb == 32);
    CHECK(o.max_open_files == 256);
    CHECK(o.enable_wal == true);
    CHECK(o.sync_wal == true);
    CHECK(o.block_size_kb == 16);
    CHECK(o.bloom_bits_per_key == 8);
    CHECK(o.compaction_interval_sec == 300);
    CHECK(lsm_config_validate(&o) == LSM_OK);
    lsm_options_destroy(&o);

    unlink(path);
}

static void test_file_missing(void) {
    LsmOptions o = lsm_config_from_file("/tmp/lsm_test_config_does_not_exist.cfg");
    CHECK(o.db_path != NULL);
    CHECK(o.memtable_size_mb == 4);
    CHECK(o.owns_db_path == false);
    CHECK(lsm_config_validate(&o) == LSM_OK);
    lsm_options_destroy(&o);
}

static void test_file_ignores_comments(void) {
    const char *path = "/tmp/lsm_test_config_comments.cfg";
    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    fprintf(f, "memtable_size_mb = 16\n");
    fprintf(f, "no_equals_sign_line\n");
    fprintf(f, "max_open_files = 128\n");
    fclose(f);

    LsmOptions o = lsm_config_from_file(path);
    CHECK(o.memtable_size_mb == 16);
    CHECK(o.max_open_files == 128);
    lsm_options_destroy(&o);

    unlink(path);
}

static void test_validate_invalid(void) {
    LsmOptions o = lsm_options_default();
    CHECK(lsm_config_validate(&o) == LSM_OK);

    o.db_path = NULL;
    CHECK(lsm_config_validate(&o) == LSM_ERR_INVALID_ARG);

    o = lsm_options_default();
    o.db_path = "";
    CHECK(lsm_config_validate(&o) == LSM_ERR_INVALID_ARG);

    o = lsm_options_default();
    o.memtable_size_mb = 0;
    CHECK(lsm_config_validate(&o) == LSM_ERR_INVALID_ARG);

    o = lsm_options_default();
    o.memtable_size_mb = 2048;
    CHECK(lsm_config_validate(&o) == LSM_ERR_INVALID_ARG);

    o = lsm_options_default();
    o.max_open_files = 5;
    CHECK(lsm_config_validate(&o) == LSM_ERR_INVALID_ARG);

    o = lsm_options_default();
    o.max_open_files = 100000;
    CHECK(lsm_config_validate(&o) == LSM_ERR_INVALID_ARG);

    o = lsm_options_default();
    o.block_size_kb = 0;
    CHECK(lsm_config_validate(&o) == LSM_ERR_INVALID_ARG);

    o = lsm_options_default();
    o.block_size_kb = 100000;
    CHECK(lsm_config_validate(&o) == LSM_ERR_INVALID_ARG);

    o = lsm_options_default();
    o.bloom_bits_per_key = 0;
    CHECK(lsm_config_validate(&o) == LSM_ERR_INVALID_ARG);

    o = lsm_options_default();
    o.bloom_bits_per_key = 100;
    CHECK(lsm_config_validate(&o) == LSM_ERR_INVALID_ARG);

    o = lsm_options_default();
    o.compaction_interval_sec = 100000;
    CHECK(lsm_config_validate(&o) == LSM_ERR_INVALID_ARG);
}

int main(void) {
    clear_env();
    test_defaults();
    test_env_override();
    test_env_falsy();
    test_file_override();
    test_file_missing();
    test_file_ignores_comments();
    test_validate_invalid();
    puts("test_config: OK");
    return 0;
}
