#ifndef LSM_TYPES_H
#define LSM_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef uint64_t SequenceNumber;
typedef int (*LsmComparator)(const void *a, size_t len_a, const void *b, size_t len_b);

typedef struct {
    const char *data;
    size_t len;
} LsmSlice;

static inline LsmSlice lsm_slice(const char *data, size_t len) {
    LsmSlice s;
    s.data = data;
    s.len = len;
    return s;
}

static inline LsmSlice lsm_slice_str(const char *str) {
    return lsm_slice(str, strlen(str));
}

static inline int lsm_slice_cmp(LsmSlice a, LsmSlice b) {
    size_t min_len = (a.len < b.len) ? a.len : b.len;
    int r = memcmp(a.data, b.data, min_len);
    if (r != 0)
        return r;
    if (a.len < b.len)
        return -1;
    if (a.len > b.len)
        return 1;
    return 0;
}

static inline bool lsm_slice_eq(LsmSlice a, LsmSlice b) {
    return a.len == b.len && memcmp(a.data, b.data, a.len) == 0;
}

typedef struct {
    LsmSlice user_key;
    SequenceNumber seq;
} LsmInternalKey;

static inline LsmSlice lsm_internal_key_slice(const LsmInternalKey *ik) {
    return lsm_slice(ik->user_key.data, ik->user_key.len);
}

#endif
