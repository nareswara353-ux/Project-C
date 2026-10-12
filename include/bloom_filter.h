#ifndef LSM_BLOOM_FILTER_H
#define LSM_BLOOM_FILTER_H

#include "lsm_types.h"

typedef struct BloomFilter BloomFilter;

BloomFilter *bloom_filter_create(uint32_t num_entries, double false_positive_rate);
void bloom_filter_destroy(BloomFilter *bf);
void bloom_filter_add(BloomFilter *bf, LsmSlice key);
bool bloom_filter_may_contain(const BloomFilter *bf, LsmSlice key);
size_t bloom_filter_serialize(const BloomFilter *bf, uint8_t *out, size_t out_len);
BloomFilter *bloom_filter_deserialize(const uint8_t *data, size_t len);

#endif
