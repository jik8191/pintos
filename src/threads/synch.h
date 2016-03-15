/*! \file synch.h
 *
 * Data structures and function declarations for thread synchronization
 * primitives.
 */

#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/*! A counting semaphore. */
struct semaphore {
    unsigned value;             /*!< Current value. */
    struct list waiters;        /*!< List of waiting threads. */
};

void sema_init(struct semaphore *, unsigned value);
void sema_down(struct semaphore *);
bool sema_try_down(struct semaphore *);
void sema_up(struct semaphore *);
void sema_self_test(void);

/*! Lock. */
struct lock {
    struct thread *holder;      /*!< Thread holding lock (for debugging). */
    struct semaphore semaphore; /*!< Binary semaphore controlling access. */

    struct list_elem elem;      /*!< The list elem for a thread's list of locks. */
    int donated_priority;       /*!< The highest priority donated by a thread. */
};

void lock_init(struct lock *);
void lock_acquire(struct lock *);
void donate_priority(struct lock *, int priority);
bool lock_try_acquire(struct lock *);
void lock_release(struct lock *);
bool lock_held_by_current_thread(const struct lock *);

/*! Condition variable. */
struct condition {
    struct list waiters; /*!< List of semaphores holding waiting threads. */
};

void cond_init(struct condition *);
void cond_wait(struct condition *, struct lock *);
void cond_signal(struct condition *, struct lock *);
void cond_broadcast(struct condition *, struct lock *);

struct rwlock {
    struct lock lock;

    struct condition reader_cond;
    struct condition writer_cond;

    int readers;
    int writers;

    int waiting_readers;
    int waiting_writers;
};

void rwlock_init(struct rwlock *);
void rwlock_acquire_reader(struct rwlock *);
void rwlock_release_reader(struct rwlock *);
void rwlock_acquire_writer(struct rwlock *);
void rwlock_release_writer(struct rwlock *);

/* A function that returns if threads A's priority is greater than B's */
bool waiting_pri_higher(const struct list_elem *a, const struct list_elem *b,
        void *aux);

/* A function that returns the semaphore with the highest priority waiting
   thread. */
bool sema_waiters_pri_higher(const struct list_elem *a,
        const struct list_elem *b, void *aux);

/* A function that returns the lock with the lower donated priority. */
bool lock_donated_pri_lower(const struct list_elem *a,
        const struct list_elem *b, void *aux);

/*! Optimization barrier.

   The compiler will not reorder operations across an
   optimization barrier.  See "Optimization Barriers" in the
   reference guide for more information.*/
#define barrier() asm volatile ("" : : : "memory")

#endif /* threads/synch.h */

