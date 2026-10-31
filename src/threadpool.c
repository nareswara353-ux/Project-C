#include "internal/threadpool.h"

#include "lsm_types.h"

#include <pthread.h>
#include <stdlib.h>

typedef struct Task {
    LsmTaskFn fn;
    void *arg;
    struct Task *next;
} Task;

struct LsmThreadPool {
    pthread_mutex_t mutex;
    pthread_cond_t work_cond;
    pthread_cond_t idle_cond;
    pthread_t *workers;
    size_t worker_count;
    Task *head;
    Task *tail;
    size_t pending;
    size_t active;
    bool shutdown;
};

static void *worker_loop(void *arg) {
    LsmThreadPool *pool = arg;
    for (;;) {
        pthread_mutex_lock(&pool->mutex);
        while (!pool->shutdown && pool->head == NULL)
            pthread_cond_wait(&pool->work_cond, &pool->mutex);

        if (pool->shutdown && pool->head == NULL) {
            pthread_mutex_unlock(&pool->mutex);
            return NULL;
        }

        Task *task = pool->head;
        pool->head = task->next;
        if (pool->head == NULL)
            pool->tail = NULL;
        pool->pending--;
        pool->active++;
        pthread_mutex_unlock(&pool->mutex);

        task->fn(task->arg);
        free(task);

        pthread_mutex_lock(&pool->mutex);
        pool->active--;
        if (pool->pending == 0 && pool->active == 0)
            pthread_cond_broadcast(&pool->idle_cond);
        pthread_mutex_unlock(&pool->mutex);
    }
}

LsmThreadPool *lsm_threadpool_create(size_t worker_count) {
    if (worker_count == 0)
        return NULL;
    LsmThreadPool *pool = calloc(1, sizeof(LsmThreadPool));
    if (!pool)
        return NULL;

    if (pthread_mutex_init(&pool->mutex, NULL) != 0) {
        free(pool);
        return NULL;
    }
    if (pthread_cond_init(&pool->work_cond, NULL) != 0) {
        pthread_mutex_destroy(&pool->mutex);
        free(pool);
        return NULL;
    }
    if (pthread_cond_init(&pool->idle_cond, NULL) != 0) {
        pthread_cond_destroy(&pool->work_cond);
        pthread_mutex_destroy(&pool->mutex);
        free(pool);
        return NULL;
    }

    pool->workers = calloc(worker_count, sizeof(pthread_t));
    if (!pool->workers) {
        pthread_cond_destroy(&pool->idle_cond);
        pthread_cond_destroy(&pool->work_cond);
        pthread_mutex_destroy(&pool->mutex);
        free(pool);
        return NULL;
    }

    pool->worker_count = worker_count;
    pool->head = NULL;
    pool->tail = NULL;
    pool->pending = 0;
    pool->active = 0;
    pool->shutdown = false;

    for (size_t i = 0; i < worker_count; i++) {
        if (pthread_create(&pool->workers[i], NULL, worker_loop, pool) != 0) {
            pool->worker_count = i;
            lsm_threadpool_destroy(pool);
            return NULL;
        }
    }

    return pool;
}

LsmStatus lsm_threadpool_submit(LsmThreadPool *pool, LsmTaskFn fn, void *arg) {
    if (!pool || !fn)
        return LSM_ERR_INVALID_ARG;

    Task *task = malloc(sizeof(Task));
    if (!task)
        return LSM_ERR_MEMORY;
    task->fn = fn;
    task->arg = arg;
    task->next = NULL;

    pthread_mutex_lock(&pool->mutex);
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->mutex);
        free(task);
        return LSM_ERR_BUSY;
    }
    if (pool->tail)
        pool->tail->next = task;
    else
        pool->head = task;
    pool->tail = task;
    pool->pending++;
    pthread_cond_signal(&pool->work_cond);
    pthread_mutex_unlock(&pool->mutex);
    return LSM_OK;
}

void lsm_threadpool_wait(LsmThreadPool *pool) {
    if (!pool)
        return;
    pthread_mutex_lock(&pool->mutex);
    while (pool->pending > 0 || pool->active > 0)
        pthread_cond_wait(&pool->idle_cond, &pool->mutex);
    pthread_mutex_unlock(&pool->mutex);
}

size_t lsm_threadpool_pending(const LsmThreadPool *pool) {
    if (!pool)
        return 0;
    LsmThreadPool *mutable_pool = (LsmThreadPool *)pool;
    pthread_mutex_lock(&mutable_pool->mutex);
    size_t pending = pool->pending;
    pthread_mutex_unlock(&mutable_pool->mutex);
    return pending;
}

void lsm_threadpool_destroy(LsmThreadPool *pool) {
    if (!pool)
        return;

    pthread_mutex_lock(&pool->mutex);
    pool->shutdown = true;
    pthread_cond_broadcast(&pool->work_cond);
    pthread_mutex_unlock(&pool->mutex);

    for (size_t i = 0; i < pool->worker_count; i++)
        pthread_join(pool->workers[i], NULL);

    Task *cur = pool->head;
    while (cur) {
        Task *next = cur->next;
        free(cur);
        cur = next;
    }
    pool->head = NULL;
    pool->tail = NULL;

    pthread_cond_destroy(&pool->idle_cond);
    pthread_cond_destroy(&pool->work_cond);
    pthread_mutex_destroy(&pool->mutex);
    free(pool->workers);
    free(pool);
}
