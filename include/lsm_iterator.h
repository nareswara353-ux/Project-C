#ifndef LSM_ITERATOR_H
#define LSM_ITERATOR_H

#include "lsm_tree.h"
#include "lsm_types.h"

#include <stddef.h>

typedef struct LsmIteratorOps {
    bool (*valid)(void *state);
    void (*next)(void *state);
    LsmSlice (*key)(void *state);
    LsmSlice (*value)(void *state);
    void (*destroy)(void *state);
} LsmIteratorOps;

typedef struct {
    const LsmIteratorOps *ops;
    void *state;
} LsmIteratorImpl;

LsmIterator *lsm_iterator_from_impl(const LsmIteratorOps *ops, void *state);
LsmIterator *lsm_merge_iterator(LsmIterator **iters, size_t count);
LsmIterator *lsm_concat_iterator(LsmIterator **iters, size_t count);

#endif
