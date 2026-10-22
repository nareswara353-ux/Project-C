#include "lsm_iterator.h"

#include "lsm_types.h"

#include <stdlib.h>

struct LsmIterator {
    const LsmIteratorOps *ops;
    void *state;
};

LsmIterator *lsm_iterator_from_impl(const LsmIteratorOps *ops, void *state) {
    LsmIterator *it = malloc(sizeof(struct LsmIterator));
    if (!it)
        return NULL;
    it->ops = ops;
    it->state = state;
    return it;
}

bool lsm_iterator_valid(const LsmIterator *it) {
    if (!it || !it->ops || !it->ops->valid)
        return false;
    return it->ops->valid(it->state);
}

void lsm_iterator_next(LsmIterator *it) {
    if (!it || !it->ops || !it->ops->next)
        return;
    it->ops->next(it->state);
}

LsmSlice lsm_iterator_key(const LsmIterator *it) {
    if (!it || !it->ops || !it->ops->key)
        return (LsmSlice){NULL, 0};
    return it->ops->key(it->state);
}

LsmSlice lsm_iterator_value(const LsmIterator *it) {
    if (!it || !it->ops || !it->ops->value)
        return (LsmSlice){NULL, 0};
    return it->ops->value(it->state);
}

void lsm_iterator_destroy(LsmIterator *it) {
    if (!it)
        return;
    if (it->ops && it->ops->destroy && it->state)
        it->ops->destroy(it->state);
    free(it);
}

typedef struct {
    LsmIterator **children;
    size_t count;
} MergeState;

static size_t merge_min_index(const MergeState *m) {
    size_t min_idx = (size_t)-1;
    LsmSlice min_key = {NULL, 0};
    for (size_t i = 0; i < m->count; i++) {
        if (!lsm_iterator_valid(m->children[i]))
            continue;
        LsmSlice k = lsm_iterator_key(m->children[i]);
        if (min_idx == (size_t)-1 || lsm_slice_cmp(k, min_key) < 0) {
            min_idx = i;
            min_key = k;
        }
    }
    return min_idx;
}

static bool merge_valid(void *state) {
    const MergeState *m = state;
    for (size_t i = 0; i < m->count; i++) {
        if (lsm_iterator_valid(m->children[i]))
            return true;
    }
    return false;
}

static void merge_next(void *state) {
    MergeState *m = state;
    size_t idx = merge_min_index(m);
    if (idx != (size_t)-1)
        lsm_iterator_next(m->children[idx]);
}

static LsmSlice merge_key(void *state) {
    MergeState *m = state;
    size_t idx = merge_min_index(m);
    if (idx == (size_t)-1)
        return (LsmSlice){NULL, 0};
    return lsm_iterator_key(m->children[idx]);
}

static LsmSlice merge_value(void *state) {
    MergeState *m = state;
    size_t idx = merge_min_index(m);
    if (idx == (size_t)-1)
        return (LsmSlice){NULL, 0};
    return lsm_iterator_value(m->children[idx]);
}

static void merge_destroy(void *state) {
    MergeState *m = state;
    if (!m)
        return;
    for (size_t i = 0; i < m->count; i++) {
        if (m->children[i])
            lsm_iterator_destroy(m->children[i]);
    }
    free(m->children);
    free(m);
}

static const LsmIteratorOps merge_ops = {
    .valid = merge_valid,
    .next = merge_next,
    .key = merge_key,
    .value = merge_value,
    .destroy = merge_destroy,
};

LsmIterator *lsm_merge_iterator(LsmIterator **iters, size_t count) {
    if (!iters || count == 0)
        return NULL;
    MergeState *m = malloc(sizeof(MergeState));
    if (!m)
        return NULL;
    m->children = malloc(count * sizeof(LsmIterator *));
    if (!m->children) {
        free(m);
        return NULL;
    }
    for (size_t i = 0; i < count; i++)
        m->children[i] = iters[i];
    m->count = count;
    return lsm_iterator_from_impl(&merge_ops, m);
}

typedef struct {
    LsmIterator **children;
    size_t count;
    size_t current;
} ConcatState;

static bool concat_valid(void *state) {
    ConcatState *c = state;
    while (c->current < c->count) {
        if (lsm_iterator_valid(c->children[c->current]))
            return true;
        c->current++;
    }
    return false;
}

static void concat_next(void *state) {
    ConcatState *c = state;
    if (c->current < c->count)
        lsm_iterator_next(c->children[c->current]);
}

static LsmSlice concat_key(void *state) {
    ConcatState *c = state;
    if (c->current >= c->count)
        return (LsmSlice){NULL, 0};
    return lsm_iterator_key(c->children[c->current]);
}

static LsmSlice concat_value(void *state) {
    ConcatState *c = state;
    if (c->current >= c->count)
        return (LsmSlice){NULL, 0};
    return lsm_iterator_value(c->children[c->current]);
}

static void concat_destroy(void *state) {
    ConcatState *c = state;
    if (!c)
        return;
    for (size_t i = 0; i < c->count; i++) {
        if (c->children[i])
            lsm_iterator_destroy(c->children[i]);
    }
    free(c->children);
    free(c);
}

static const LsmIteratorOps concat_ops = {
    .valid = concat_valid,
    .next = concat_next,
    .key = concat_key,
    .value = concat_value,
    .destroy = concat_destroy,
};

LsmIterator *lsm_concat_iterator(LsmIterator **iters, size_t count) {
    if (!iters || count == 0)
        return NULL;
    ConcatState *c = malloc(sizeof(ConcatState));
    if (!c)
        return NULL;
    c->children = malloc(count * sizeof(LsmIterator *));
    if (!c->children) {
        free(c);
        return NULL;
    }
    for (size_t i = 0; i < count; i++)
        c->children[i] = iters[i];
    c->count = count;
    c->current = 0;
    return lsm_iterator_from_impl(&concat_ops, c);
}
