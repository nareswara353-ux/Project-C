#include "lsm_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

typedef struct SkipNode {
    LsmInternalKey key;
    LsmSlice value;
    struct SkipNode* next[];
} SkipNode;

typedef struct SkipList {
    int max_level;
    SkipNode* head;
    LsmComparator cmp;
    atomic_size_t size;
} SkipList;

static int random_level_skip(int max) {
    int level = 1;
    while ((rand() & 1) && level < max) level++;
    return level;
}

static SkipNode* node_create(const LsmInternalKey* key, LsmSlice value, int level) {
    SkipNode* n = malloc(sizeof(SkipNode) + (level + 1) * sizeof(SkipNode*));
    if (!n) return NULL;
    n->key = *key;
    n->value = value;
    for (int i = 0; i <= level; i++) n->next[i] = NULL;
    return n;
}

SkipList* skiplist_create(LsmComparator cmp, int max_level) {
    SkipList* list = malloc(sizeof(SkipList));
    if (!list) return NULL;
    list->max_level = max_level;
    list->cmp = cmp;
    atomic_init(&list->size, 0);
    LsmInternalKey zero_key = {{NULL, 0}, 0};
    list->head = node_create(&zero_key, (LsmSlice){NULL, 0}, max_level);
    if (!list->head) {
        free(list);
        return NULL;
    }
    return list;
}

void skiplist_destroy(SkipList* list) {
    if (!list) return;
    SkipNode* cur = list->head;
    while (cur) {
        SkipNode* next = cur->next[0];
        free(cur);
        cur = next;
    }
    free(list);
}

LsmStatus skiplist_insert(SkipList* list, LsmInternalKey key, LsmSlice value) {
    if (!list) return LSM_ERR_INVALID_ARG;
    SkipNode* update[list->max_level + 1];
    SkipNode* cur = list->head;
    for (int i = list->max_level; i >= 0; i--) {
        while (cur->next[i] && list->cmp(cur->next[i]->key.user_key.data, cur->next[i]->key.user_key.len,
                                         key.user_key.data, key.user_key.len) < 0) {
            cur = cur->next[i];
        }
        update[i] = cur;
    }
    cur = cur->next[0];
    if (cur && list->cmp(cur->key.user_key.data, cur->key.user_key.len,
                         key.user_key.data, key.user_key.len) == 0) {
        cur->value = value;
        return LSM_OK;
    }
    int level = random_level_skip(list->max_level);
    SkipNode* n = node_create(&key, value, level);
    if (!n) return LSM_ERR_MEMORY;
    for (int i = 0; i <= level; i++) {
        n->next[i] = update[i]->next[i];
        update[i]->next[i] = n;
    }
    atomic_fetch_add(&list->size, 1);
    return LSM_OK;
}

LsmStatus skiplist_find(SkipList* list, LsmSlice key, LsmSlice* out_value) {
    if (!list || !out_value) return LSM_ERR_INVALID_ARG;
    SkipNode* cur = list->head->next[0];
    while (cur) {
        int cmp = list->cmp(cur->key.user_key.data, cur->key.user_key.len, key.data, key.len);
        if (cmp == 0) {
            *out_value = cur->value;
            return LSM_OK;
        }
        cur = cur->next[0];
    }
    return LSM_ERR_NOT_FOUND;
}

size_t skiplist_size(SkipList* list) {
    if (!list) return 0;
    return atomic_load(&list->size);
}

SkipNode* skiplist_begin(SkipList* list) {
    if (!list) return NULL;
    return list->head->next[0];
}

SkipNode* skiplist_next(SkipNode* node) {
    if (!node) return NULL;
    return node->next[0];
}

LsmSlice skiplist_node_key(SkipNode* node) {
    if (!node) return (LsmSlice){NULL, 0};
    return lsm_internal_key_slice(&node->key);
}

LsmSlice skiplist_node_value(SkipNode* node) {
    if (!node) return (LsmSlice){NULL, 0};
    return node->value;
}
