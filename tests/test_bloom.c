#include "lsm_tree.h"
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

typedef struct BloomFilter BloomFilter;

extern BloomFilter *bloom_filter_create(uint32_t num_entries, double false_positive_rate);
extern void bloom_filter_destroy(BloomFilter *bf);
extern void bloom_filter_add(BloomFilter *bf, LsmSlice key);
extern bool bloom_filter_may_contain(const BloomFilter *bf, LsmSlice key);
extern size_t bloom_filter_serialize(const BloomFilter *bf, uint8_t *out, size_t out_len);
extern BloomFilter *bloom_filter_deserialize(const uint8_t *data, size_t len);

static void test_bloom_contains(void) {
    BloomFilter *bf = bloom_filter_create(1000, 0.01);
    CHECK(bf != NULL);

    const char *keys[] = {"alpha", "beta", "gamma", "delta", "epsilon"};
    size_t n = sizeof(keys) / sizeof(keys[0]);
    for (size_t i = 0; i < n; i++) {
        bloom_filter_add(bf, lsm_slice_str(keys[i]));
    }
    for (size_t i = 0; i < n; i++) {
        CHECK(bloom_filter_may_contain(bf, lsm_slice_str(keys[i])) == true);
    }

    bloom_filter_destroy(bf);
}

static void test_bloom_false_positive_rate(void) {
    BloomFilter *bf = bloom_filter_create(1000, 0.01);
    CHECK(bf != NULL);

    char buf[32];
    for (int i = 0; i < 1000; i++) {
        snprintf(buf, sizeof(buf), "present_%d", i);
        bloom_filter_add(bf, lsm_slice_str(buf));
    }

    int false_positives = 0;
    int trials = 10000;
    for (int i = 0; i < trials; i++) {
        snprintf(buf, sizeof(buf), "absent_%d", i);
        if (bloom_filter_may_contain(bf, lsm_slice_str(buf)))
            false_positives++;
    }

    double rate = (double)false_positives / (double)trials;
    CHECK(rate < 0.05);

    bloom_filter_destroy(bf);
}

static void test_bloom_serialize_roundtrip(void) {
    BloomFilter *bf = bloom_filter_create(100, 0.01);
    CHECK(bf != NULL);

    for (int i = 0; i < 100; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "key_%d", i);
        bloom_filter_add(bf, lsm_slice_str(buf));
    }

    size_t cap = 4096;
    uint8_t *buffer = malloc(cap);
    CHECK(buffer != NULL);
    size_t written = bloom_filter_serialize(bf, buffer, cap);
    CHECK(written > 0);

    BloomFilter *restored = bloom_filter_deserialize(buffer, written);
    CHECK(restored != NULL);

    for (int i = 0; i < 100; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "key_%d", i);
        CHECK(bloom_filter_may_contain(restored, lsm_slice_str(buf)) == true);
    }

    bloom_filter_destroy(restored);
    free(buffer);
    bloom_filter_destroy(bf);
}

static void test_bloom_null_args(void) {
    bloom_filter_destroy(NULL);
    CHECK(bloom_filter_may_contain(NULL, lsm_slice_str("x")) == false);
    bloom_filter_add(NULL, lsm_slice_str("x"));

    LsmSlice empty = {NULL, 0};
    BloomFilter *bf = bloom_filter_create(10, 0.01);
    CHECK(bf != NULL);
    bloom_filter_add(bf, empty);
    CHECK(bloom_filter_may_contain(bf, empty) == false);
    bloom_filter_destroy(bf);

    uint8_t dummy[1] = {0};
    CHECK(bloom_filter_deserialize(dummy, 0) == NULL);
    CHECK(bloom_filter_deserialize(NULL, 100) == NULL);
}

int main(void) {
    test_bloom_contains();
    test_bloom_false_positive_rate();
    test_bloom_serialize_roundtrip();
    test_bloom_null_args();
    puts("test_bloom: OK");
    return 0;
}
