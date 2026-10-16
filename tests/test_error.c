#include "lsm_error.h"
#include "lsm_types.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check_impl(int ok, const char *expr, const char *file, int line) {
    if (!ok) {
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", file, line, expr);
        exit(1);
    }
}

#define CHECK(cond) check_impl((cond) ? 1 : 0, #cond, __FILE__, __LINE__)

static void test_status_strings(void) {
    CHECK(strcmp(lsm_status_string(LSM_OK), "ok") == 0);
    CHECK(strcmp(lsm_status_string(LSM_ERR_IO), "io error") == 0);
    CHECK(strcmp(lsm_status_string(LSM_ERR_CORRUPTION), "corruption detected") == 0);
    CHECK(strcmp(lsm_status_string(LSM_ERR_NOT_FOUND), "key not found") == 0);
    CHECK(strcmp(lsm_status_string(LSM_ERR_FULL), "storage full") == 0);
    CHECK(strcmp(lsm_status_string(LSM_ERR_INVALID_ARG), "invalid argument") == 0);
    CHECK(strcmp(lsm_status_string(LSM_ERR_MEMORY), "out of memory") == 0);
    CHECK(strcmp(lsm_status_string(LSM_ERR_BUSY), "resource busy") == 0);
    CHECK(strcmp(lsm_status_string((LsmStatus)9999), "unknown error") == 0);
}

static void test_predicates(void) {
    CHECK(lsm_status_is_ok(LSM_OK) == true);
    CHECK(lsm_status_is_ok(LSM_ERR_IO) == false);
    CHECK(lsm_status_is_ok(LSM_ERR_NOT_FOUND) == false);

    CHECK(lsm_status_is_error(LSM_OK) == false);
    CHECK(lsm_status_is_error(LSM_ERR_IO) == true);
    CHECK(lsm_status_is_error(LSM_ERR_MEMORY) == true);
}

static void test_errno_conversion(void) {
    CHECK(lsm_status_from_errno(0) == LSM_OK);
    CHECK(lsm_status_from_errno(ENOMEM) == LSM_ERR_MEMORY);
    CHECK(lsm_status_from_errno(EINVAL) == LSM_ERR_INVALID_ARG);
    CHECK(lsm_status_from_errno(ENOENT) == LSM_ERR_NOT_FOUND);
    CHECK(lsm_status_from_errno(ENOSPC) == LSM_ERR_FULL);
    CHECK(lsm_status_from_errno(EBUSY) == LSM_ERR_BUSY);
    CHECK(lsm_status_from_errno(EIO) == LSM_ERR_IO);
    CHECK(lsm_status_from_errno(99999) == LSM_ERR_IO);
}

static void test_print_no_crash(void) {
    lsm_status_print(LSM_OK, "test");
    lsm_status_print(LSM_ERR_IO, "test");
    lsm_status_print(LSM_ERR_NOT_FOUND, NULL);
    lsm_status_print((LsmStatus)12345, "unknown");
}

int main(void) {
    test_status_strings();
    test_predicates();
    test_errno_conversion();
    test_print_no_crash();
    puts("test_error: OK");
    return 0;
}
