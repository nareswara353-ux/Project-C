#include "internal/checksum.h"

#include "lsm_types.h"

#include <stdint.h>
#include <string.h>

#define CRC32C_POLY 0x82F63B78u
#define CRC32C_INIT 0xFFFFFFFFu

uint32_t lsm_checksum_crc32c_extend(uint32_t seed, const void *data, size_t len) {
    const uint8_t *p = data;
    uint32_t crc = seed ^ CRC32C_INIT;
    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 1)
                crc = (crc >> 1) ^ CRC32C_POLY;
            else
                crc >>= 1;
        }
    }
    return crc ^ CRC32C_INIT;
}

uint32_t lsm_checksum_crc32c(const void *data, size_t len) {
    return lsm_checksum_crc32c_extend(0, data, len);
}

bool lsm_checksum_verify(const void *data, size_t len, uint32_t expected) {
    return lsm_checksum_crc32c(data, len) == expected;
}
