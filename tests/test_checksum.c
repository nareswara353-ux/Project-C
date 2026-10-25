#include "internal/checksum.h"
#include "lsm_types.h"

#include <stdint.h>
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

static void test_known_vector(void) {
    const char *digits = "123456789";
    uint32_t crc = lsm_checksum_crc32c(digits, strlen(digits));
    CHECK(crc == 0xE3069283u);
}

static void test_empty_buffer(void) {
    uint32_t crc = lsm_checksum_crc32c("", 0);
    CHECK(crc == 0u);
}

static void test_extend_matches_oneshot(void) {
    const char *data = "hello world, this is a checksum test payload";
    size_t len = strlen(data);
    uint32_t one_shot = lsm_checksum_crc32c(data, len);

    uint32_t partial1 = lsm_checksum_crc32c_extend(0, data, 11);
    uint32_t partial2 = lsm_checksum_crc32c_extend(partial1, data + 11, len - 11);
    CHECK(partial2 == one_shot);

    uint32_t bytewise = 0;
    for (size_t i = 0; i < len; i++)
        bytewise = lsm_checksum_crc32c_extend(bytewise, data + i, 1);
    CHECK(bytewise == one_shot);
}

static void test_verify(void) {
    const char *data = "verify me";
    size_t len = strlen(data);
    uint32_t expected = lsm_checksum_crc32c(data, len);
    CHECK(lsm_checksum_verify(data, len, expected) == true);
    CHECK(lsm_checksum_verify(data, len, expected ^ 0x1u) == false);
}

static void test_binary_payload(void) {
    uint8_t buf[256];
    for (int i = 0; i < 256; i++)
        buf[i] = (uint8_t)i;
    uint32_t a = lsm_checksum_crc32c(buf, sizeof(buf));
    uint32_t b = lsm_checksum_crc32c(buf, sizeof(buf));
    CHECK(a == b);
    CHECK(a != 0u);
    CHECK(lsm_checksum_verify(buf, sizeof(buf), a) == true);

    buf[0] = 0xFF;
    CHECK(lsm_checksum_verify(buf, sizeof(buf), a) == false);
}

int main(void) {
    test_known_vector();
    test_empty_buffer();
    test_extend_matches_oneshot();
    test_verify();
    test_binary_payload();
    puts("test_checksum: OK");
    return 0;
}
