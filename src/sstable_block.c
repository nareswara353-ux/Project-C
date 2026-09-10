#include "lsm_tree.h"
#include "lsm_types.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint32_t crc32_block(const void *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t *bytes = data;
    for (size_t i = 0; i < len; i++) {
        crc ^= bytes[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}

size_t block_max_compressed_len(size_t input_len) {
    return input_len + 16;
}

LsmStatus block_compress(const void *input, size_t input_len, void *output, size_t *output_len) {
    if (!input || !output || !output_len)
        return LSM_ERR_INVALID_ARG;
    if (*output_len < input_len)
        return LSM_ERR_INVALID_ARG;
    memcpy(output, input, input_len);
    *output_len = input_len;
    return LSM_OK;
}

LsmStatus block_decompress(const void *input, size_t input_len, void *output, size_t *output_len) {
    if (!input || !output || !output_len)
        return LSM_ERR_INVALID_ARG;
    if (*output_len < input_len)
        return LSM_ERR_INVALID_ARG;
    memcpy(output, input, input_len);
    *output_len = input_len;
    return LSM_OK;
}

uint32_t block_checksum(const void *data, size_t len) {
    return crc32_block(data, len);
}

bool block_checksum_verify(const void *data, size_t len, uint32_t expected) {
    return block_checksum(data, len) == expected;
}
