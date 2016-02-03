/*! \file synch.c
 *
 * Implementation of various thread synchronization primitives.
 */

/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/*! Initializes semaphore SEMA to VALUE.  A semaphore is a
    nonnegative integer along with two atomic operators for
    manipulating it:

    - down or "P": wait for the value to become positive, then
      decrement it.

    - up or "V": increment the value (and wake up one waiting
      thread, if any). */
void sema_init(struct semaphore *sema, unsigned value) {
    ASSERT(sema != NULL);

    sema->value = value;
    list_init(&sema->waiters);
}

/*! Down or "P" operation on a semaphore.  Waits for SEMA's value
    to become positive and then atomically decrements it.

    This function may sleep, so it must not be called within an
    interrupt handler.  This function may be called with
    interrupts disabled, but if it sleeps then the next scheduled
    thread will probably turn interrupts back on. */
void sema_down(struct semaphore *sema) {
    enum intr_level old_level;

    ASSERT(sema != NULL);
    ASSERT(!intr_context());

    old_level = intr_disable();
    while (sema->value == 0) {
        /* list_insert_ordered(&sema->waiters, &thread_current()->semaelem, */
        /*                     waiting_pri_higher, NULL); */
        list_push_back(&sema->waiters, &thread_current()->semaelem);
        thread_block();
    }
    sema->value--;
    intr_set_level(old_level);
}

/*! Down or "P" operation on a semaphore, but only if the
    semaphore is not already 0.  Returns true if the semaphore is
    decremented, false otherwise.

    This function may be called from an interrupt handler. */
bool sema_try_down(struct semaphore *sema) {
    enum intr_level old_level;
    bool success;

    ASSERT(sema != NULL);

    old_level = intr_disable();
    if (sema->value > 0) {
        sema->value--;
        success = true;
    }
    else {
      success = false;
    }
    intr_set_level(old_level);

    return success;
}

/*! Up or "V" operation on a semaphore.  Increments SEMA's value
    and wakes up one thread of those waiting for SEMA, if any.

    This function may be called from an interrupt handler. */
void sema_up(struct semaphore *sema) {
    enum intr_level old_level;

    ASSERT(sema != NULL);

    old_level = intr_disable();
    sema->value++;
    if (!list_empty(&sema->waiters)) {
        /* struct list_elem *e = list_max(&sema->waiters, waiting_pri_higher, NULL); */
        /* thread_unblock(list_entry(e, struct thread, semaelem)); */
        list_sort(&sema->waiters, waiting_pri_higher, NULL);
        thread_unblock(
            list_entry(
                list_pop_front(&sema->waiters), struct thread, semaelem));
    }
    intr_set_level(old_level);
}

static void sema_test_helper(void *sema_);

/*! Self-test for semaphores that makes control "ping-pong"
    between a pair of threads.  Insert calls to printf() to see
    what's going on. */
void sema_self_test(void) {
    struct semaphore sema[2];
    int i;

    printf("Testing semaphores...");
    sema_init(&sema[0], 0);
    sema_init(&sema[1], 0);
    thread_create("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
    for (i = 0; i < 10; i++) {
        sema_up(&sema[0]);
        sema_down(&sema[1]);
    }
    printf ("done.\n");
}

/*! Thread function used by sema_self_test(). */
static void sema_test_helper(void *sema_) {
    struct semaphore *sema = sema_;
    int i;

    for (i = 0; i < 10; i++) {
        sema_down(&sema[0]);
        sema_up(&sema[1]);
    }
}

/*! Initializes LOCK.  A lock can be held by at most a single
    thread at any given time.  Our locks are not "recursive", that
    is, it is an error for the thread currently holding a lock to
    try to acquire that lock.

    A lock is a specialization of a semaphore with an initial
    value of 1.  The difference between a lock and such a
    semaphore is twofold.  First, a semaphore can have a value
    greater than 1, but a lock can only be owned by a single
    thread at a time.  Second, a semaphore does not have an owner,
    meaning that one thread can "down" the semaphore and then
    another one "up" it, but with a lock the same thread must both
    acquire and release it.  When these restrictions prove
    onerous, it's a good sign that a semaphore should be used,
    instead of a lock. */
void lock_init(struct lock *lock) {
    ASSERT(lock != NULL);

    lock->holder = NULL;
    lock->donated_priority = PRI_MIN;
    sema_init(&lock->semaphore, 1);
}

/*! Acquires LOCK, sleeping until it becomes available if
    necessary.  The lock must not already be held by the current
    thread.

    This function may sleep, so it must not be called within an
    interrupt handler.  This function may be called with
    interrupts disabled, but interrupts will be turned back on if
    we need to sleep. */
void lock_acquire(struct lock *lock) {
    ASSERT(lock != NULL);
    ASSERT(!intr_context());
    ASSERT(!lock_held_by_current_thread(lock));

    enum intr_level old_level = intr_disable();

    struct thread *t = thread_current();

    // Donate priority to the lock's holder by giving it to the lock.
    /* donate_priority(lock, t->priority); */
    donate_priority(lock, thread_get_priority());

    // For now, this thread is waiting on the lock.
    t->lock_waiton = lock;

    // Now use the underlying semaphore to wait on the lock, if needed.
    sema_down(&lock->semaphore);

    // Now the thread has acquired the lock.
    t->lock_waiton = NULL;
    lock->holder = t;

    // When the lock is released to this thread, the semaphore should relase it
    // to the thread with the highest waiting priority, so there should be no
    // more threads currently waiting with a higher priority that can donate.
    lock->donated_priority = t->priority;

    // Keep track of all locks held by a thread.
    list_push_back(&t->locks, &lock->elem);

    // TODO: This might be an issue that hangs the program since we might block
    // a thread while interrupts are disabled.
    intr_set_level(old_level);
}

/*! Donates priority to a lock so that any thread holding that lock can hold a
    higher priority and release it quicker.

    This should work for nested locks (e.g. if a thread is waiting on a lock
    that is held by another thread that is waiting on a different lock, then
    the holder of that final lock should get donated the highest priority). By
    "parent" thread in the comments below, we would mean any thread that is
    waiting on a lock that is held by you or any other "parent" thread. */
void donate_priority(struct lock *lock, int priority) {
    // Check to see if we can help free the lock faster by donating priority.
    if (priority > lock->donated_priority) {
        lock->donated_priority = priority;
    }

    // Check to see if the holder of the lock we are waiting on is waiting for
    // a nested lock and a "parent" thread has donated a higher priority than
    // that nested thread.
    struct thread *nested_t = lock->holder;
    if (nested_t != NULL && nested_t->priority < priority) {

        // Change the lock holder's priority.
        /* nested_t->priority = priority; */

        // Move the lock holder into a new ready_queue, if it is not running.
        if (nested_t->status == THREAD_READY) {
            thread_reschedule(nested_t, priority);
        }

        struct lock *nested_l = nested_t->lock_waiton;

        if (nested_l != NULL) {
            // Donate the "parent" thread's priority to the nested lock.
            donate_priority(nested_l, priority);
        }
    }
}

/*! Tries to acquires LOCK and returns true if successful or false
    on failure.  The lock must not already be held by the current
    thread.

    This function will not sleep, so it may be called within an
    interrupt handler. */
bool lock_try_acquire(struct lock *lock) {
    bool success;

    ASSERT(lock != NULL);
    ASSERT(!lock_held_by_current_thread(lock));

    success = sema_try_down(&lock->semaphore);
    if (success)
      lock->holder = thread_current();

    return success;
}

/*! Releases LOCK, which must be owned by the current thread.

    An interrupt handler cannot acquire a lock, so it does not
    make sense to try to release a lock within an interrupt
    handler. */
void lock_release(struct lock *lock) {
    ASSERT(lock != NULL);
    ASSERT(lock_held_by_current_thread(lock));
    ASSERT(!intr_context());

    enum intr_level old_level = intr_disable();

    lock->holder = NULL;
    list_remove(&lock->elem);

    /* thread_current()->priority = thread_get_priority(); */

    /*
    // If the thread is holding any locks, its priority is the highest of any
    // donated priority from a thread waiting on one of those locks.
    struct thread *curr = thread_current();

    if (!list_empty(&curr->locks)) {
        struct lock *max_pri_l = list_entry(
                list_max(&curr->locks, lock_donated_pri_lower, NULL),
                struct lock, elem);

        if (max_pri_l->donated_priority > curr->init_priority) {
            thread_current()->priority = max_pri_l->donated_priority;
        } else {
            curr->priority = curr->init_priority;
        }
    } else {
        curr->priority = curr->init_priority;
    }
    */

    intr_set_level(old_level);

    sema_up(&lock->semaphore);
}

/*! Returns true if the current thread holds LOCK, false
    otherwise.  (Note that testing whether some other thread holds
    a lock would be racy.) */
bool lock_held_by_current_thread(const struct lock *lock) {
    ASSERT(lock != NULL);

    return lock->holder == thread_current();
}

/*! One semaphore in a list. */
struct semaphore_elem {
    struct list_elem elem;              /*!< List element. */
    struct semaphore semaphore;         /*!< This semaphore. */
};

/*! Initializes condition variable COND.  A condition variable
    allows one piece of code to signal a condition and cooperating
    code to receive the signal and act upon it. */
void cond_init(struct condition *cond) {
    ASSERT(cond != NULL);

    list_init(&cond->waiters);
}

/*! Atomically releases LOCK and waits for COND to be signaled by
    some other piece of code.  After COND is signaled, LOCK is
    reacquired before returning.  LOCK must be held before calling
    this function.

    The monitor implemented by this function is "Mesa" style, not
    "Hoare" style, that is, sending and receiving a signal are not
    an atomic operation.  Thus, typically the caller must recheck
    the condition after the wait completes and, if necessary, wait
    again.

    A given condition variable is associated with only a single
    lock, but one lock may be associated with any number of
    condition variables.  That is, there is a one-to-many mapping
    from locks to condition variables.

    This function may sleep, so it must not be called within an
    interrupt handler.  This function may be called with
    interrupts disabled, but interrupts will be turned back on if
    we need to sleep. */
void cond_wait(struct condition *cond, struct lock *lock) {
    struct semaphore_elem waiter;

    ASSERT(cond != NULL);
    ASSERT(lock != NULL);
    ASSERT(!intr_context());
    ASSERT(lock_held_by_current_thread(lock));

    sema_init(&waiter.semaphore, 0);
    list_push_back(&cond->waiters, &waiter.elem);
    lock_release(lock);
    sema_down(&waiter.semaphore);
    lock_acquire(lock);
}

/*! If any threads are waiting on COND (protected by LOCK), then
    this function signals one of them to wake up from its wait.
    LOCK must be held before calling this function.

    An interrupt handler cannot acquire a lock, so it does not
    make sense to try to signal a condition variable within an
    interrupt handler. */
void cond_signal(struct condition *cond, struct lock *lock UNUSED) {
    ASSERT(cond != NULL);
    ASSERT(lock != NULL);
    ASSERT(!intr_context ());
    ASSERT(lock_held_by_current_thread (lock));

    if (!list_empty(&cond->waiters)) {
        list_sort(&cond->waiters, sema_waiters_pri_higher, NULL);
        sema_up(&list_entry(list_pop_front(&cond->waiters),
                            struct semaphore_elem, elem)->semaphore);
    }
}

/*! Wakes up all threads, if any, waiting on COND (protected by
    LOCK).  LOCK must be held before calling this function.

    An interrupt handler cannot acquire a lock, so it does not
    make sense to try to signal a condition variable within an
    interrupt handler. */
void cond_broadcast(struct condition *cond, struct lock *lock) {
    ASSERT(cond != NULL);
    ASSERT(lock != NULL);

    while (!list_empty(&cond->waiters))
        cond_signal(cond, lock);
}

/*! A function that returns if threads A's priority is less than B's */
bool waiting_pri_higher(const struct list_elem *a, const struct list_elem *b,
        void *aux UNUSED) {

    struct thread *f = list_entry (a, struct thread, semaelem);
    struct thread *g = list_entry (b, struct thread, semaelem);
    /* return f->priority >= g->priority; */
    return thread_get_priority_t(f) >= thread_get_priority_t(g);
}

/*! A function that returns if one semaphore's waiter has a higher priority
    than another's. */
bool sema_waiters_pri_higher(const struct list_elem *a,
        const struct list_elem *b, void *aux UNUSED) {
    struct semaphore *s, *t;
    s = &list_entry(a, struct semaphore_elem, elem)->semaphore;
    t = &list_entry(b, struct semaphore_elem, elem)->semaphore;

    ASSERT(list_size(&s->waiters) == 1);
    ASSERT(list_size(&t->waiters) == 1);

    return waiting_pri_higher(list_front(&s->waiters), list_front(&t->waiters),
                              NULL);
}

/*! A function that returns true if lock a has a lower donated priority than
    lock b. */
bool lock_donated_pri_lower(const struct list_elem *a,
        const struct list_elem *b, void *aux UNUSED) {
    struct lock *l, *k;
    l = list_entry(a, struct lock, elem);
    k = list_entry(b, struct lock, elem);

    return l->donated_priority < k->donated_priority;
}
