#ifndef LSM_INTERNAL_CHECKSUM_H
#define LSM_INTERNAL_CHECKSUM_H

#include "lsm_types.h"

#include <stddef.h>
#include <stdint.h>

uint32_t lsm_checksum_crc32c(const void *data, size_t len);
uint32_t lsm_checksum_crc32c_extend(uint32_t seed, const void *data, size_t len);
bool lsm_checksum_verify(const void *data, size_t len, uint32_t expected);

#endif
