/*
 * sem.h -- unnamed semaphores for macOS
 *
 * This uses pthread_mutex and pthread_cond to make a custom semaphore type.
 * Inspired by Brian's answer https://stackoverflow.com/a/48778462.
 * Implemented from scratch by Keno Hassler, 2023 (copyright CC-0).
 */

#include <pthread.h>
#include <sys/errno.h>

typedef struct sem {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    unsigned count;
} sem_t;

int sem_init(sem_t *sem, int pshared, unsigned value);
int sem_destroy(sem_t *sem);
int sem_wait(sem_t *sem);
int sem_post(sem_t *sem);
