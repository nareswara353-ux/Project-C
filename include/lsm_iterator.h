#ifndef LSM_ITERATOR_H
#define LSM_ITERATOR_H

#include <stddef.h>
#include "lsm_tree.h"

LsmIterator* lsm_merge_iterator(LsmIterator** iters, size_t count);
LsmIterator* lsm_concat_iterator(LsmIterator** iters, size_t count);

#endif
