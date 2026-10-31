#include "internal/threadpool.h"
#include "lsm_types.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

static void check_impl(int ok, const char *expr, const char *file, int line) {
    if (!ok) {
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", file, line, expr);
        exit(1);
    }
}

#define CHECK(cond) check_impl((cond) ? 1 : 0, #cond, __FILE__, __LINE__)

typedef struct {
    atomic_int *counter;
    int increment;
} TaskArg;

static void bump_counter(void *arg) {
    TaskArg *t = arg;
    atomic_fetch_add(t->counter, t->increment);
}

typedef struct {
    atomic_int *counter;
    pthread_mutex_t *mutex;
    int *observed_max;
    int *observed_min;
} SyncTaskArg;

static void heavy_task(void *arg) {
    SyncTaskArg *t = arg;
    int value = atomic_fetch_add(t->counter, 1);
    pthread_mutex_lock(t->mutex);
    if (value > *t->observed_max)
        *t->observed_max = value;
    if (value < *t->observed_min)
        *t->observed_min = value;
    pthread_mutex_unlock(t->mutex);
}

static void test_basic_submit_wait(void) {
    LsmThreadPool *pool = lsm_threadpool_create(2);
    CHECK(pool != NULL);

    atomic_int counter = 0;
    TaskArg arg = {.counter = &counter, .increment = 1};

    for (int i = 0; i < 50; i++)
        CHECK(lsm_threadpool_submit(pool, bump_counter, &arg) == LSM_OK);

    lsm_threadpool_wait(pool);
    CHECK(atomic_load(&counter) == 50);
    CHECK(lsm_threadpool_pending(pool) == 0);

    lsm_threadpool_destroy(pool);
}

static void test_no_task(void) {
    LsmThreadPool *pool = lsm_threadpool_create(1);
    CHECK(pool != NULL);
    lsm_threadpool_wait(pool);
    CHECK(lsm_threadpool_pending(pool) == 0);
    lsm_threadpool_destroy(pool);
}

static void test_multiple_workers(void) {
    LsmThreadPool *pool = lsm_threadpool_create(4);
    CHECK(pool != NULL);

    atomic_int counter = 0;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    int observed_max = -1;
    int observed_min = 1 << 30;

    SyncTaskArg arg = {
        .counter = &counter,
        .mutex = &mutex,
        .observed_max = &observed_max,
        .observed_min = &observed_min,
    };

    for (int i = 0; i < 100; i++)
        CHECK(lsm_threadpool_submit(pool, heavy_task, &arg) == LSM_OK);

    lsm_threadpool_wait(pool);
    CHECK(atomic_load(&counter) == 100);
    CHECK(observed_max == 99);
    CHECK(observed_min == 0);

    pthread_mutex_destroy(&mutex);
    lsm_threadpool_destroy(pool);
}

static void test_pending_query(void) {
    LsmThreadPool *pool = lsm_threadpool_create(1);
    CHECK(pool != NULL);

    atomic_int counter = 0;
    TaskArg arg = {.counter = &counter, .increment = 0};

    for (int i = 0; i < 20; i++)
        CHECK(lsm_threadpool_submit(pool, bump_counter, &arg) == LSM_OK);

    lsm_threadpool_wait(pool);
    CHECK(lsm_threadpool_pending(pool) == 0);

    lsm_threadpool_destroy(pool);
}

static void test_null_args(void) {
    CHECK(lsm_threadpool_create(0) == NULL);

    LsmThreadPool *pool = lsm_threadpool_create(1);
    CHECK(pool != NULL);
    CHECK(lsm_threadpool_submit(pool, NULL, NULL) == LSM_ERR_INVALID_ARG);
    CHECK(lsm_threadpool_submit(NULL, bump_counter, NULL) == LSM_ERR_INVALID_ARG);
    CHECK(lsm_threadpool_pending(NULL) == 0);
    lsm_threadpool_wait(NULL);
    lsm_threadpool_destroy(NULL);
    lsm_threadpool_destroy(pool);
}

int main(void) {
    test_basic_submit_wait();
    test_no_task();
    test_multiple_workers();
    test_pending_query();
    test_null_args();
    puts("test_threadpool: OK");
    return 0;
}
