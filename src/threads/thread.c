#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/timer.h"
#ifdef USERPROG
#include "userprog/process.h"
#include "userprog/syscall.h"
#endif

/*! Random value for struct thread's `magic' member.
    Used to detect stack overflow.  See the big comment at the top
    of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/*! List processes in THREAD_BLOCKED state, that is, processes
    that are blocked because they have been made to sleep. */
static struct list wait_list;

/*! List of processes in THREAD_READY state, that is, processes
    that are ready to run but not actually running. */
static struct list ready_lists[PRI_MAX - PRI_MIN + 1];

/*! List of all processes.  Processes are added to this list
    when they are first scheduled and removed when they exit. */
static struct list all_list;

/*! Idle thread. */
static struct thread *idle_thread;

/*! Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/*! Lock used by allocate_tid(). */
static struct lock tid_lock;

/*! Lock used for ready_lists */
static struct lock ready_lock;

/*! Stack frame for kernel_thread(). */
struct kernel_thread_frame {
    void *eip;                  /*!< Return address. */
    thread_func *function;      /*!< Function to call. */
    void *aux;                  /*!< Auxiliary data for function. */
};

/* Statistics. */
static long long idle_ticks;    /*!< # of timer ticks spent idle. */
static long long kernel_ticks;  /*!< # of timer ticks in kernel threads. */
static long long user_ticks;    /*!< # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /*!< # of timer ticks to give each thread. */
static unsigned thread_ticks;   /*!< # of timer ticks since last yield. */

/* The number of threads that are ready or running */
static unsigned num_threads_ready;

/*! If false (default), use round-robin scheduler.
    If true, use multi-level feedback queue scheduler.
    Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread(thread_func *, void *aux);

static void idle(void *aux UNUSED);
static struct thread *running_thread(void);
static struct thread *next_thread_to_run(void);
static void init_thread(struct thread *, const char *name, int priority);
static bool is_thread(struct thread *) UNUSED;
static void *alloc_frame(struct thread *, size_t size);
static void schedule(void);
void thread_schedule_tail(struct thread *prev);
static tid_t allocate_tid(void);

/*! Initializes the threading system by transforming the code
    that's currently running into a thread.  This can't work in
    general and it is possible in this case only because loader.S
    was careful to put the bottom of the stack at a page boundary.

    Also initializes the run queue and the tid lock.

    After calling this function, be sure to initialize the page allocator
    before trying to create any threads with thread_create().

    It is not safe to call thread_current() until this function finishes. */
void thread_init(void) {
    ASSERT(intr_get_level() == INTR_OFF);

    lock_init(&tid_lock);
    lock_init(&ready_lock);
    /* Initialize the array of ready_lists */
    int i = PRI_MIN;
    for (; i <= PRI_MAX; i++) {
        list_init(&ready_lists[i]);
    }

    list_init(&wait_list);
    list_init(&all_list);

    num_threads_ready = 0;

    /* Set up a thread structure for the running thread. */
    initial_thread = running_thread();
    init_thread(initial_thread, "main", PRI_DEFAULT);
    initial_thread->status = THREAD_RUNNING;
    initial_thread->tid = allocate_tid();

    if (thread_mlfqs) {
        // Initializing the load_avg
        init_load_avg();
        // Setting the inital threads nice value
        initial_thread->nice = NICE_DEFAULT;
        // Setting its recent cpu value
        initial_thread->recent_cpu = int_to_fp(0);
        // Setting the priority
        thread_calculate_priority(initial_thread);
    }
}

/*! Starts preemptive thread scheduling by enabling interrupts.
    Also creates the idle thread. */
void thread_start(void) {
    /* Create the idle thread. */
    struct semaphore idle_started;
    sema_init(&idle_started, 0);
    thread_create("idle", PRI_MIN, idle, &idle_started);

    /* Start preemptive thread scheduling. */
    intr_enable();

    /* Wait for the idle thread to initialize idle_thread. */
    sema_down(&idle_started);
}

/*! Called by the timer interrupt handler at each timer tick.
    Thus, this function runs in an external interrupt context. */
void thread_tick(void) {
    struct thread *t = thread_current();

    /* Update statistics. */
    if (t == idle_thread)
        idle_ticks++;
#ifdef USERPROG
    else if (t->pagedir != NULL)
        user_ticks++;
        if (thread_mlfqs) {
            t->recent_cpu = int_add(t->recent_cpu, 1);
        }
#endif
    else {
        kernel_ticks++;
        /* If mlqfs then update recent cpu */
        if (thread_mlfqs) {
            t->recent_cpu = int_add(t->recent_cpu, 1);
        }
    }

    /* Enforce preemption. */
    if (++thread_ticks >= TIME_SLICE)
        intr_yield_on_return();
}

/*! Prints thread statistics. */
void thread_print_stats(void) {
    printf("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
           idle_ticks, kernel_ticks, user_ticks);
}

/*! Creates a new kernel thread named NAME with the given initial PRIORITY,
    which executes FUNCTION passing AUX as the argument, and adds it to the
    ready queue.  Returns the thread identifier for the new thread, or
    TID_ERROR if creation fails.

    If thread_start() has been called, then the new thread may be scheduled
    before thread_create() returns.  It could even exit before thread_create()
    returns.  Contrariwise, the original thread may run for any amount of time
    before the new thread is scheduled.  Use a semaphore or some other form of
    synchronization if you need to ensure ordering.

    The code provided sets the new thread's `priority' member to PRIORITY, but
    no actual priority scheduling is implemented.  Priority scheduling is the
    goal of Problem 1-3. */
tid_t thread_create(const char *name, int priority, thread_func *function,
                    void *aux) {
    struct thread *t;
    struct kernel_thread_frame *kf;
    struct switch_entry_frame *ef;
    struct switch_threads_frame *sf;
    tid_t tid;

    ASSERT(function != NULL);

    /* Allocate thread. */
    t = palloc_get_page(PAL_ZERO);
    if (t == NULL)
        return TID_ERROR;

    /* Initialize thread. */
    init_thread(t, name, priority);
    tid = t->tid = allocate_tid();

    /* Setting up the nice value if mlfqs */
    if (thread_mlfqs) {
        t->nice = thread_current()->nice;
        t->recent_cpu = thread_current()->recent_cpu;
        thread_calculate_priority(t);
    }

    /* Stack frame for kernel_thread(). */
    kf = alloc_frame(t, sizeof *kf);
    kf->eip = NULL;
    kf->function = function;
    kf->aux = aux;

    /* Stack frame for switch_entry(). */
    ef = alloc_frame(t, sizeof *ef);
    ef->eip = (void (*) (void)) kernel_thread;

    /* Stack frame for switch_threads(). */
    sf = alloc_frame(t, sizeof *sf);
    sf->eip = switch_entry;
    sf->ebp = 0;

    /* Add to run queue. */
    thread_unblock(t);

    return tid;
}

/*! Puts the current thread to sleep.  It will not be scheduled
    again until awoken by thread_unblock().

    This function must be called with interrupts turned off.  It is usually a
    better idea to use one of the synchronization primitives in synch.h. */
void thread_block(void) {
    ASSERT(!intr_context());
    ASSERT(intr_get_level() == INTR_OFF);

    thread_current()->status = THREAD_BLOCKED;
    schedule();
}

/*! Transitions a blocked thread T to the ready-to-run state.  This is an
    error if T is not blocked.  (Use thread_yield() to make the running
    thread ready.)

    This function does not preempt the running thread.  This can be important:
    if the caller had disabled interrupts itself, it may expect that it can
    atomically unblock a thread and update other data. */
void thread_unblock(struct thread *t) {
    enum intr_level old_level;

    ASSERT(is_thread(t));

    old_level = intr_disable();

    ASSERT(t->status == THREAD_BLOCKED);
    list_push_back(&ready_lists[thread_get_priority_t(t)], &t->rdyelem);
    num_threads_ready++;
    t->status = THREAD_READY;

    /* If the current running thread is of lower priority than a new thread
       that is about to be unblocked, then yield the current thread */
    if (thread_get_priority() < t->priority) {

        // We don't want an interrupt handler to yield.
        if (!intr_context()) {
            thread_yield();
        }
    }

    intr_set_level(old_level);
}

/*! Returns the name of the running thread. */
const char * thread_name(void) {
    return thread_current()->name;
}

/*! Returns the running thread.
    This is running_thread() plus a couple of sanity checks.
    See the big comment at the top of thread.h for details. */
struct thread * thread_current(void) {
    struct thread *t = running_thread();

    /* Make sure T is really a thread.
       If either of these assertions fire, then your thread may
       have overflowed its stack.  Each thread has less than 4 kB
       of stack, so a few big automatic arrays or moderate
       recursion can cause stack overflow. */
    ASSERT(is_thread(t));
    ASSERT(t->status == THREAD_RUNNING);

    return t;
}

/*! Returns the running thread's tid. */
tid_t thread_tid(void) {
    return thread_current()->tid;
}

/*! Deschedules the current thread and destroys it.  Never
    returns to the caller. */
void thread_exit(void) {
    ASSERT(!intr_context());
    struct thread *cur = thread_current();

#ifdef USERPROG
    if (cur->userprog) {
        printf("%s: exit(%d)\n", cur->name, cur->return_status);
    }

    /* Cleaning up the file descriptors */
    struct list_elem *e = list_begin(&cur->fd_list);
    while (e != list_end(&cur->fd_list)) {
        struct fd_elem *curr_fd = list_entry(e, struct fd_elem, elem);
        e = list_next(e);
        sys_close(curr_fd->fd);
    }

    /* Cleaning up the mapped files */
    e = list_begin(&cur->mmap_files);
    while (e != list_end(&cur->mmap_files)) {
        struct mmap_fileinfo *mf = list_entry(e, struct mmap_fileinfo, elem);
        e = list_next(e);
        sys_munmap(mf->mapid);
    }

    // Allow parent waiting to run.
    sema_up(&cur->child_wait);

    process_exit();
#endif

    /* Remove thread from all threads list, set our status to dying,
       and schedule another process.  That process will destroy us
       when it calls thread_schedule_tail(). */
    intr_disable();
    list_remove(&cur->allelem);
    thread_current()->status = THREAD_DYING;
    schedule();
    NOT_REACHED();
}

/*! Yields the CPU.  The current thread is not put to sleep and
    may be scheduled again immediately at the scheduler's whim. */
void thread_yield(void) {
    struct thread *cur = thread_current();
    enum intr_level old_level;

    ASSERT(!intr_context());

    old_level = intr_disable();

    if (cur != idle_thread) {
        list_push_back(&ready_lists[thread_get_priority()], &cur->rdyelem);
        num_threads_ready++;
    }
    cur->status = THREAD_READY;
    schedule();

    intr_set_level(old_level);
}


/*! Helper function for thread_sleep, return whether thread f should
    wake up before thread g so the list is in order,
    with the head having the earliest wake time. */
static bool awake_earlier (const struct list_elem *a, const struct list_elem *b,
        void *aux UNUSED){

    struct thread *f = list_entry (a, struct thread, waitelem);
    struct thread *g = list_entry (b, struct thread, waitelem);

    if (f->ticks_awake != g->ticks_awake){
        return f->ticks_awake < g->ticks_awake;
    } else {
        // check priority
        return f->priority > g->priority;
    }
}

void thread_sleep(struct thread *t){
    ASSERT(intr_get_level() == INTR_ON);

    enum intr_level old_level = intr_disable();
    list_insert_ordered(&wait_list, &(t->waitelem), awake_earlier, NULL);
    intr_set_level(old_level);

    sema_down(&t->sema_wait);
}

void threads_wake(int64_t ticks_now){
    // The wait list has the thread with the earliest
    // wake time in front, so go over the list until we find a thread
    // with a later wake up time than the current time,
    while (!list_empty(&wait_list)){
        struct list_elem *welem_thr = list_front(&wait_list);
        struct thread *thr = list_entry (welem_thr, struct thread, waitelem);

        if (thr->ticks_awake <= ticks_now) {
            sema_up(&thr->sema_wait);      // wake this thread
            list_pop_front(&wait_list);    // remove it from the list
        }
        else {
            break;
        }
    }
}

/*! Invoke function 'func' on all threads, passing along 'aux'.
    This function must be called with interrupts off. */
void thread_foreach(thread_action_func *func, void *aux) {
    struct list_elem *e;

    ASSERT(intr_get_level() == INTR_OFF);

    for (e = list_begin(&all_list); e != list_end(&all_list);
         e = list_next(e)) {
        struct thread *t = list_entry(e, struct thread, allelem);
        func(t, aux);
    }
}

/*! Sets the current thread's priority to NEW_PRIORITY. */
void thread_set_priority(int new_priority) {
    if (thread_mlfqs) {
        return;
    }

    /* Make sure the value is valid, or it would segfault array access */
    ASSERT(new_priority <= PRI_MAX);
    ASSERT(new_priority >= PRI_MIN);

    thread_current()->priority = new_priority;

    lock_acquire(&ready_lock);
    /* Check if there are any threads in a higher queue that want to run */
    int i = PRI_MAX;
    for (; i > new_priority; i--) {
        if (!list_empty(&ready_lists[i])) {
            lock_release(&ready_lock);
            thread_yield();
            break;
        }
    }
    if (lock_held_by_current_thread(&ready_lock)) {
        lock_release(&ready_lock);
    }

}

/*! Returns the current thread's priority. */
int thread_get_priority(void) {
    if (thread_mlfqs) {
        return thread_current()->priority;
    }

    return thread_get_priority_t(thread_current());
}

int thread_get_priority_t(struct thread *t) {
    int priority = t->priority;

    // If the thread is holding any locks, its priority is the highest of any
    // donated priority from a thread waiting on one of those locks.
    if (!list_empty(&t->locks)) {
        struct lock *max_pri_l = list_entry(
                list_max(&t->locks, lock_donated_pri_lower, NULL),
                struct lock, elem);

        if (max_pri_l->donated_priority > priority)
            return max_pri_l->donated_priority;
    }

    return priority;
}

/*! Move a ready thread from its old ready queue to a new one depending on its
    current priority. */
void thread_reschedule(struct thread *t, int priority) {
    if (!intr_context()) {
        lock_acquire(&ready_lock);
    }
    struct list_elem *rdyelem = &t->rdyelem;
    list_remove(rdyelem);
    struct list *ready_list = &ready_lists[priority];
    list_push_back(ready_list, rdyelem);
    if (!intr_context()) {
        lock_release(&ready_lock);
    }
}

/*! Sets the current thread's nice value to NICE. */
void thread_set_nice(int nice) {
    if (nice > NICE_MAX) {
        nice = NICE_MAX;
    } else if (nice < NICE_MIN) {
        nice = NICE_MIN;
    }

    thread_current()->nice = nice;
    thread_calculate_priority(thread_current());

    /* Check if there are any threads in a higher queue that want to run */
    int i = PRI_MAX;
    lock_acquire(&ready_lock);
    for (; i > thread_get_priority(); i--) {
        if (!list_empty(&ready_lists[i])) {
            lock_release(&ready_lock);
            thread_yield();
            break;
        }
    }
    if (lock_held_by_current_thread(&ready_lock)) {
        lock_release(&ready_lock);
    }

}

/*! Returns the current thread's nice value. */
int thread_get_nice(void) {
    return thread_current()->nice;
}

/*! Returns 100 times the system load average. */
int thread_get_load_avg(void) {
    /* Not yet implemented. */
    return fp_to_int(get_load_avg(), 1) * 100;
}

/*! Returns 100 times the current thread's recent_cpu value. */
int thread_get_recent_cpu(void) {
    /* Not yet implemented. */
    return fp_to_int(thread_current()->recent_cpu, 1) * 100;
}

/* Calculates a threads priority */
void thread_calculate_priority(struct thread *t) {
    int new_priority = PRI_MAX;

    new_priority = new_priority - fp_to_int(int_divide(t->recent_cpu, 4), 0);
    new_priority = new_priority - (t->nice * 2);

    if (new_priority < PRI_MIN) {
        new_priority = PRI_MIN;
    }

    if (new_priority > PRI_MAX) {
        new_priority = PRI_MAX;
    }

    // If the priority changed and the thread was in the ready lists, move it.
    if (new_priority != t->priority && t->status == THREAD_READY) {
        thread_reschedule(t, new_priority);
    }

    t->priority = new_priority;
}

/*! Returns the total number of threads that are running */
int threads_ready(void) {
    if (thread_current() != idle_thread)
        return num_threads_ready + 1;

    return num_threads_ready;
}

// Returns whether the multi-level feedback queue scheduler is being used
bool get_mlfqs(void) {
    return thread_mlfqs;
}

bool is_idle_thread(struct thread *t) {
    return t == idle_thread;
}

/* Returns a list of all threads */
struct list *get_all_list(void) {
    return &all_list;
}

/* A function that returns a thread pointer given a tid */
struct thread *get_thread(tid_t tid) {
    struct list_elem *e = list_begin(&all_list);

    for (; e != list_end(&all_list); e = list_next(e)) {
        struct thread *t = list_entry(e, struct thread, allelem);

        if (t->tid == tid) {
            return t;
        }

    }

    return NULL;
}

/*! Idle thread.  Executes when no other thread is ready to run.

    The idle thread is initially put on the ready list by thread_start().
    It will be scheduled once initially, at which point it initializes
    idle_thread, "up"s the semaphore passed to it to enable thread_start()
    to continue, and immediately blocks.  After that, the idle thread never
    appears in the ready list.  It is returned by next_thread_to_run() as a
    special case when the ready list is empty. */
static void idle(void *idle_started_ UNUSED) {
    struct semaphore *idle_started = idle_started_;
    idle_thread = thread_current();
    sema_up(idle_started);

    for (;;) {
        /* Let someone else run. */
        intr_disable();
        thread_block();

        /* Re-enable interrupts and wait for the next one.

           The `sti' instruction disables interrupts until the completion of
           the next instruction, so these two instructions are executed
           atomically.  This atomicity is important; otherwise, an interrupt
           could be handled between re-enabling interrupts and waiting for the
           next one to occur, wasting as much as one clock tick worth of time.

           See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
           7.11.1 "HLT Instruction". */
        asm volatile ("sti; hlt" : : : "memory");
    }
}

/*! Function used as the basis for a kernel thread. */
static void kernel_thread(thread_func *function, void *aux) {
    ASSERT(function != NULL);

    intr_enable();       /* The scheduler runs with interrupts off. */
    function(aux);       /* Execute the thread function. */
    thread_exit();       /* If function() returns, kill the thread. */
}

/*! Returns the running thread. */
struct thread * running_thread(void) {
    uint32_t *esp;

    /* Copy the CPU's stack pointer into `esp', and then round that
       down to the start of a page.  Because `struct thread' is
       always at the beginning of a page and the stack pointer is
       somewhere in the middle, this locates the curent thread. */
    asm ("mov %%esp, %0" : "=g" (esp));
    return pg_round_down(esp);
}

/*! Returns true if T appears to point to a valid thread. */
static bool is_thread(struct thread *t) {
    return t != NULL && t->magic == THREAD_MAGIC;
}

/*! Does basic initialization of T as a blocked thread named NAME. */
static void init_thread(struct thread *t, const char *name, int priority) {
    enum intr_level old_level;

    ASSERT(t != NULL);
    ASSERT(PRI_MIN <= priority && priority <= PRI_MAX);
    ASSERT(name != NULL);

    memset(t, 0, sizeof *t);
    t->status = THREAD_BLOCKED;
    strlcpy(t->name, name, sizeof t->name);
    t->stack = (uint8_t *) t + PGSIZE;
    t->priority = priority;
    t->magic = THREAD_MAGIC;

    sema_init(&(t->sema_wait), 0);  // Thread is initially not blocked

    // Initialize the list of locks held by the thread (starts empty).
    list_init(&(t->locks));
    t->lock_waiton = NULL;

    // Initialize the list of file descriptors held by the thread.
    list_init(&(t->fd_list));

    // Initialize the list of memory mapped files held by the thread.
    list_init(&(t->mmap_files));

#ifdef USERPROG
    // Initialize the list of child process information.
    list_init(&(t->children));

    // Initialize the semaphore used to wait on children.
    sema_init(&(t->child_wait), 0);

    t->return_status = -1;
    t->userprog = false;
#endif

    /* The max fd starts off at 1 */
    t->max_fd = 1;

    /* Start with 0 memory mapped files */
    t->num_mfiles = 0;
    old_level = intr_disable();
    list_push_back(&all_list, &t->allelem);
    intr_set_level(old_level);
}

/*! Allocates a SIZE-byte frame at the top of thread T's stack and
    returns a pointer to the frame's base. */
static void * alloc_frame(struct thread *t, size_t size) {
    /* Stack data is always allocated in word-size units. */
    ASSERT(is_thread(t));
    ASSERT(size % sizeof(uint32_t) == 0);

    t->stack -= size;
    return t->stack;
}

/*! Chooses and returns the next thread to be scheduled.  Should return a
    thread from the run queue, unless the run queue is empty.  (If the running
    thread can continue running, then it will be in the run queue.)  If the
    run queue is empty, return idle_thread. */
static struct thread * next_thread_to_run(void) {
    int i = PRI_MAX;
    for (; i >= PRI_MIN; i--) {
        struct list *ready_lst = &ready_lists[i];

        if (!list_empty(ready_lst)) {
            struct thread *next =
                list_entry(list_pop_front(ready_lst), struct thread, rdyelem);
            num_threads_ready--;
            return next;
        }
    }

    return idle_thread;
}

/*! Completes a thread switch by activating the new thread's page tables, and,
    if the previous thread is dying, destroying it.

    At this function's invocation, we just switched from thread PREV, the new
    thread is already running, and interrupts are still disabled.  This
    function is normally invoked by thread_schedule() as its final action
    before returning, but the first time a thread is scheduled it is called by
    switch_entry() (see switch.S).

    It's not safe to call printf() until the thread switch is complete.  In
    practice that means that printf()s should be added at the end of the
    function.

    After this function and its caller returns, the thread switch is complete.
*/
void thread_schedule_tail(struct thread *prev) {
    struct thread *cur = running_thread();

    ASSERT(intr_get_level() == INTR_OFF);

    /* Mark us as running. */
    cur->status = THREAD_RUNNING;

    /* Start new time slice. */
    thread_ticks = 0;

#ifdef USERPROG
    /* Activate the new address space. */
    process_activate();
#endif

    /* If the thread we switched from is dying, destroy its struct thread.
       This must happen late so that thread_exit() doesn't pull out the rug
       under itself.  (We don't free initial_thread because its memory was
       not obtained via palloc().) */
    if (prev != NULL && prev->status == THREAD_DYING &&
        prev != initial_thread) {
        ASSERT(prev != cur);
        palloc_free_page(prev);
    }
}

/*! Schedules a new process.  At entry, interrupts must be off and the running
    process's state must have been changed from running to some other state.
    This function finds another thread to run and switches to it.

    It's not safe to call printf() until thread_schedule_tail() has
    completed. */
static void schedule(void) {
    struct thread *cur = running_thread();
    struct thread *next = next_thread_to_run();
    struct thread *prev = NULL;

    ASSERT(intr_get_level() == INTR_OFF);
    ASSERT(cur->status != THREAD_RUNNING);
    ASSERT(is_thread(next));

    if (cur != next)
        prev = switch_threads(cur, next);
    thread_schedule_tail(prev);
}

/*! Returns a tid to use for a new thread. */
static tid_t allocate_tid(void) {
    static tid_t next_tid = 1;
    tid_t tid;

    lock_acquire(&tid_lock);
    tid = next_tid++;
    lock_release(&tid_lock);

    return tid;
}

/*! Offset of `stack' member within `struct thread'.
    Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof(struct thread, stack);

