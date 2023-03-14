#include <unistd.h>
#if _POSIX_SEMAPHORES <= 0
#include "sem.h"

int sem_init(sem_t *sem, int pshared, unsigned value) {
    if (pshared != 0) {
        // we do not support process-shared semaphores
        errno = ENOSYS;
        return -1;
    }
    int ret;

    ret = pthread_mutex_init(&sem->lock, NULL);
    if (ret != 0) {
        errno = ret;
        return -1;
    }

    ret = pthread_cond_init(&sem->cond, NULL);
    if (ret != 0) {
        errno = ret;
        return -1;
    }

    sem->count = value;
    return 0;
}

int sem_destroy(sem_t *sem) {
    int ret;

    ret = pthread_mutex_destroy(&sem->lock);
    if (ret != 0) {
        errno = ret;
        return -1;
    }

    ret = pthread_cond_destroy(&sem->cond);
    if (ret != 0) {
        errno = ret;
        return -1;
    }

    return 0;
}

int sem_wait(sem_t *sem) {
    int ret;

    ret = pthread_mutex_lock(&sem->lock);
    if (ret != 0) {
        errno = ret;
        return -1;
    }

    // safely check the count
    if (sem->count == 0) {
        ret = pthread_cond_wait(&sem->cond, &sem->lock);
        if (ret != 0) {
            errno = ret;
            return -1;
        }
    }

    // now, count > 0
    sem->count -= 1;
    ret = pthread_mutex_unlock(&sem->lock);
    if (ret != 0) {
        errno = ret;
        return -1;
    }

    return 0;
}

int sem_post(sem_t *sem) {
    int ret;

    ret = pthread_mutex_lock(&sem->lock);
    if (ret != 0) {
        errno = ret;
        return -1;
    }

    // increment and wake up other thread
    sem->count += 1;
    ret = pthread_cond_signal(&sem->cond);
    if (ret != 0) {
        errno = ret;
        return -1;
    }

    ret = pthread_mutex_unlock(&sem->lock);
    if (ret != 0) {
        errno = ret;
        return -1;
    }

    return 0;
}

#endif /* _POSIX_SEMAPHORES */
