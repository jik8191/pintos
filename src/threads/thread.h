/*! \file thread.h
 *
 * Declarations for the kernel threading functionality in PintOS.
 */

#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "synch.h"
#include <fixed_point.h>
#include <hash.h>

/*! States in a thread's life cycle. */
enum thread_status {
    THREAD_RUNNING,     /*!< Running thread. */
    THREAD_READY,       /*!< Not running but ready to run. */
    THREAD_BLOCKED,     /*!< Waiting for an event to trigger. */
    THREAD_DYING        /*!< About to be destroyed. */
};

/*! Thread identifier type.
    You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /*!< Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /*!< Lowest priority. */
#define PRI_DEFAULT 31                  /*!< Default priority. */
#define PRI_MAX 63                      /*!< Highest priority. */

#define NICE_MIN -20                    /*!< Lowest nice. */
#define NICE_DEFAULT 0                  /*!< Default nice. */
#define NICE_MAX 20                     /*!< Highest nice. */

/*! A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

\verbatim
        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+
\endverbatim

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion.

   The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list.
*/
struct thread {
    /*! Owned by thread.c. */
    /**@{*/
    tid_t tid;                          /*!< Thread identifier. */
    enum thread_status status;          /*!< Thread state. */
    char name[16];                      /*!< Name (for debugging purposes). */
    uint8_t *stack;                     /*!< Saved stack pointer. */
    int priority;                       /*!< Priority (Possible donated). */

    int nice;                           /*!< The threads nice value. */
    fp recent_cpu;                      /*!< The threads recent_cpu. */

    struct list_elem allelem;           /*!< List elem for all threads list. */
    struct list_elem rdyelem;           /*!< List elem for the ready lists. */
    struct list_elem waitelem;          /*!< List elem for waiting list. */
    /**@}*/

    /*! Shared between thread.c and synch.c. */
    /**@{*/
    int64_t ticks_awake;                /*!< Tick time to wake up at. */

    struct semaphore sema_wait;         /*!< Thread sema while sleeping. */
    struct list_elem semaelem;          /*!< List elem for sema waiters list. */

    struct list locks;                  /*!< List of locks acquired by thread */
    struct lock *lock_waiton;           /*!< Lock the thread is waiting on. */
    /**@}*/

#ifdef USERPROG
    /*! Owned by userprog/process.c. */
    /**@{*/
    uint32_t *pagedir;                  /*!< Page directory. */
    struct list fd_list;                /*!< List of file descriptors */
    int max_fd;                         /*!< Max fd the thread has */
    struct semaphore *child_sema;       /*!< A semaphore for a child to
                                             communicate with their parent */

    int *load_status;                   /*!< The location to write the return
                                             status of the thread */
    int pid;

    /* These are used for system wait calls */
    struct list children;               /*!< List of child process info */
    struct semaphore child_wait;        /*!< Semaphore used to wait for child */
    struct childinfo *info;             /*!< Info struct to put return status
                                             into upon termination */

    /* These are used for printing the exit message on termination */
    int return_status;                  /*!< The return status upon exiting */
    bool userprog;                      /*!< Whether or not this is a user
                                             spawned program. */
    /**@{*/
#endif

    struct hash spt;                    /*!< Supplemental page table */
    struct list mmap_files;             /*!< List of memory mapped file info. */
    int num_mfiles;                      /*!< Number of memory mapped files. */
    /*! Owned by thread.c. */
    /**@{*/
    unsigned magic;                     /* Detects stack overflow. */
    /**@}*/
};

/*! If false (default), use round-robin scheduler.
    If true, use multi-level feedback queue scheduler.
    Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_block(void);
void thread_unblock(struct thread *);

struct thread *thread_current (void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

void thread_sleep(struct thread *t);
void threads_wake(int64_t ticks_now);

/*! Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func(struct thread *t, void *aux);

void thread_foreach(thread_action_func *, void *);

int thread_get_priority(void);
int thread_get_priority_t(struct thread *t);
void thread_set_priority(int);

void thread_reschedule(struct thread *t, int priority);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);
bool is_idle_thread(struct thread *t);

void thread_calculate_priority(struct thread *t);

// The number of threads running or ready to run. Not including the
// idle thread
int threads_ready(void);

// Returns whether the multi-level feedback queue scheduler is being used
bool get_mlfqs(void);

struct list *get_all_list(void);

/* A function that returns a thread pointer given a tid */
struct thread *get_thread(tid_t tid);

#endif /* threads/thread.h */

