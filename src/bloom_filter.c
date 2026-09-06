#include "lsm_types.h"
#include "lsm_tree.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

typedef struct {
    uint8_t* bits;
    size_t bit_count;
    size_t byte_count;
    uint32_t num_hashes;
} BloomFilter;

static uint32_t hash_murmur(const void* key, size_t len, uint32_t seed) {
    const uint8_t* data = key;
    uint32_t h = seed;
    uint32_t k;
    for (size_t i = 0; i + 4 <= len; i += 4) {
        memcpy(&k, data + i, 4);
        k *= 0xCC9E2D51;
        k = (k << 15) | (k >> 17);
        k *= 0x1B873593;
        h ^= k;
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xE6546B64;
    }
    uint32_t remaining = 0;
    for (size_t i = len & 3; i > 0; i--) {
        remaining |= (uint32_t)data[len - i] << (8 * (i - 1));
    }
    h ^= remaining;
    h ^= len;
    h ^= h >> 16;
    h *= 0x85EBCA6B;
    h ^= h >> 13;
    h *= 0xC2B2AE35;
    h ^= h >> 16;
    return h;
}

BloomFilter* bloom_filter_create(uint32_t num_entries, double false_positive_rate) {
    BloomFilter* bf = malloc(sizeof(BloomFilter));
    if (!bf) return NULL;
    double ln2 = 0.69314718056;
    bf->num_hashes = (uint32_t)ceil(-log(false_positive_rate) / ln2);
    bf->bit_count = (size_t)ceil((double)num_entries * bf->num_hashes / ln2);
    bf->byte_count = (bf->bit_count + 7) / 8;
    bf->bits = calloc(bf->byte_count, 1);
    if (!bf->bits) {
        free(bf);
        return NULL;
    }
    return bf;
}

void bloom_filter_destroy(BloomFilter* bf) {
    if (!bf) return;
    free(bf->bits);
    free(bf);
}

void bloom_filter_add(BloomFilter* bf, LsmSlice key) {
    if (!bf || !key.data || key.len == 0) return;
    for (uint32_t i = 0; i < bf->num_hashes; i++) {
        uint32_t h = hash_murmur(key.data, key.len, i + 0x9747B28C);
        size_t bit_pos = h % bf->bit_count;
        bf->bits[bit_pos / 8] |= (uint8_t)(1 << (bit_pos % 8));
    }
}

bool bloom_filter_may_contain(const BloomFilter* bf, LsmSlice key) {
    if (!bf || !key.data || key.len == 0) return false;
    for (uint32_t i = 0; i < bf->num_hashes; i++) {
        uint32_t h = hash_murmur(key.data, key.len, i + 0x9747B28C);
        size_t bit_pos = h % bf->bit_count;
        if (!(bf->bits[bit_pos / 8] & (1 << (bit_pos % 8)))) {
            return false;
        }
    }
    return true;
}

size_t bloom_filter_serialize(const BloomFilter* bf, uint8_t* out, size_t out_len) {
    if (!bf || !out) return 0;
    size_t needed = sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t) + bf->byte_count;
    if (out_len < needed) return 0;
    uint8_t* ptr = out;
    uint32_t num_hashes = bf->num_hashes;
    uint64_t bit_count = bf->bit_count;
    uint64_t byte_count = bf->byte_count;
    memcpy(ptr, &num_hashes, sizeof(num_hashes));
    ptr += sizeof(num_hashes);
    memcpy(ptr, &bit_count, sizeof(bit_count));
    ptr += sizeof(bit_count);
    memcpy(ptr, &byte_count, sizeof(byte_count));
    ptr += sizeof(byte_count);
    memcpy(ptr, bf->bits, bf->byte_count);
    return needed;
}

BloomFilter* bloom_filter_deserialize(const uint8_t* data, size_t len) {
    if (!data || len < sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint64_t)) return NULL;
    const uint8_t* ptr = data;
    uint32_t num_hashes;
    uint64_t bit_count;
    uint64_t byte_count;
    memcpy(&num_hashes, ptr, sizeof(num_hashes));
    ptr += sizeof(num_hashes);
    memcpy(&bit_count, ptr, sizeof(bit_count));
    ptr += sizeof(bit_count);
    memcpy(&byte_count, ptr, sizeof(byte_count));
    ptr += sizeof(byte_count);
    if (len < sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint64_t) + byte_count) return NULL;
    BloomFilter* bf = malloc(sizeof(BloomFilter));
    if (!bf) return NULL;
    bf->num_hashes = num_hashes;
    bf->bit_count = (size_t)bit_count;
    bf->byte_count = (size_t)byte_count;
    bf->bits = malloc(bf->byte_count);
    if (!bf->bits) {
        free(bf);
        return NULL;
    }
    memcpy(bf->bits, ptr, bf->byte_count);
    return bf;
}
