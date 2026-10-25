#ifndef LSM_INTERNAL_THREADPOOL_H
#define LSM_INTERNAL_THREADPOOL_H

#include "lsm_types.h"

#include <stddef.h>

typedef struct LsmThreadPool LsmThreadPool;
typedef void (*LsmTaskFn)(void *arg);

LsmThreadPool *lsm_threadpool_create(size_t worker_count);
void lsm_threadpool_destroy(LsmThreadPool *pool);
LsmStatus lsm_threadpool_submit(LsmThreadPool *pool, LsmTaskFn fn, void *arg);
void lsm_threadpool_wait(LsmThreadPool *pool);
size_t lsm_threadpool_pending(const LsmThreadPool *pool);

#endif
