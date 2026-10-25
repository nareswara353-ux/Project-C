#include "internal/checksum.h"

#include "lsm_tree.h"
#include "lsm_types.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
    return lsm_checksum_crc32c(data, len);
}

bool block_checksum_verify(const void *data, size_t len, uint32_t expected) {
    return lsm_checksum_verify(data, len, expected);
}
