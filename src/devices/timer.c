/*! \file timer.c
 *
 * See [8254] for hardware details of the 8254 timer chip.
 */

#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "devices/pit.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/*! Number of timer ticks since OS booted. */
static int64_t ticks;

/* The system load average */
static fp load_avg;

/*! Number of loops per timer tick.  Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static void calculate_load_avg(void);
static void recalculate_recent_cpu(void);
static void recalculate_priorities(void);
static bool too_many_loops(unsigned loops);
static void busy_wait(int64_t loops);
static void real_time_sleep(int64_t num, int32_t denom);
static void real_time_delay(int64_t num, int32_t denom);

/*! Sets up the timer to interrupt TIMER_FREQ times per second,
    and registers the corresponding interrupt. */
void timer_init(void) {
    pit_configure_channel(0, 2, TIMER_FREQ);
    intr_register_ext(0x20, timer_interrupt, "8254 Timer");
}

/*! Sets the initial load_avg to 0. */
void init_load_avg(void) {
    load_avg.int_val = 0;
}

/* Returns the load average */
fp get_load_avg(void) {
    return load_avg;
}

/*! Calibrates loops_per_tick, used to implement brief delays. */
void timer_calibrate(void) {
    unsigned high_bit, test_bit;

    ASSERT(intr_get_level() == INTR_ON);
    printf("Calibrating timer...  ");

    /* Approximate loops_per_tick as the largest power-of-two
       still less than one timer tick. */
    loops_per_tick = 1u << 10;
    while (!too_many_loops (loops_per_tick << 1)) {
        loops_per_tick <<= 1;
        ASSERT(loops_per_tick != 0);
    }

    /* Refine the next 8 bits of loops_per_tick. */
    high_bit = loops_per_tick;
    for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1) {
        if (!too_many_loops(high_bit | test_bit))
            loops_per_tick |= test_bit;
    }

    printf("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/*! Returns the number of timer ticks since the OS booted. */
int64_t timer_ticks(void) {
    enum intr_level old_level = intr_disable();
    int64_t t = ticks;
    intr_set_level(old_level);
    return t;
}

/*! Returns the number of timer ticks elapsed since THEN, which
    should be a value once returned by timer_ticks(). */
int64_t timer_elapsed(int64_t then) {
    return timer_ticks() - then;
}


/*! Sleeps for approximately TICKS timer ticks.  Interrupts must
    be turned on. */
void timer_sleep(int64_t ticks) {
    // int64_t start = timer_ticks();
    struct thread *t_curr = thread_current();
    if (ticks > 0) {
	t_curr->ticks_awake = timer_ticks() + ticks;
    } else {
	return;  // ignore negative time
    }
    thread_sleep(t_curr);
    // while (timer_elapsed(start) < ticks)
    //	  thread_yield();
}

/*! Sleeps for approximately MS milliseconds.  Interrupts must be turned on. */
void timer_msleep(int64_t ms) {
    real_time_sleep(ms, 1000);
}

/*! Sleeps for approximately US microseconds.  Interrupts must be turned on. */
void timer_usleep(int64_t us) {
    real_time_sleep(us, 1000 * 1000);
}

/*! Sleeps for approximately NS nanoseconds.  Interrupts must be turned on. */
void timer_nsleep(int64_t ns) {
    real_time_sleep(ns, 1000 * 1000 * 1000);
}

/*! Busy-waits for approximately MS milliseconds.  Interrupts need not be
    turned on.

    Busy waiting wastes CPU cycles, and busy waiting with interrupts off for
    the interval between timer ticks or longer will cause timer ticks to be
    lost.  Thus, use timer_msleep() instead if interrupts are enabled. */
void timer_mdelay(int64_t ms) {
    real_time_delay(ms, 1000);
}

/*! Sleeps for approximately US microseconds.  Interrupts need not be turned on.

    Busy waiting wastes CPU cycles, and busy waiting with interrupts off for
    the interval between timer ticks or longer will cause timer ticks to be
    lost.  Thus, use timer_usleep() instead if interrupts are enabled. */
void timer_udelay(int64_t us) {
    real_time_delay(us, 1000 * 1000);
}

/*! Sleeps execution for approximately NS nanoseconds.  Interrupts need not be
    turned on.

    Busy waiting wastes CPU cycles, and busy waiting with interrupts off for
    the interval between timer ticks or longer will cause timer ticks to be
    lost.  Thus, use timer_nsleep() instead if interrupts are enabled. */
void timer_ndelay(int64_t ns) {
    real_time_delay(ns, 1000 * 1000 * 1000);
}

/*! Prints timer statistics. */
void timer_print_stats(void) {
    printf("Timer: %"PRId64" ticks\n", timer_ticks());
}

/*! Timer interrupt handler (interrupt service routine - ISR). */
static void timer_interrupt(struct intr_frame *args UNUSED) {
    ticks++;
    // Recalculating the load average every second and the recent cpu for all
    // threads
    // Only necessary for mlfqs.
    if (get_mlfqs() && timer_ticks() % TIMER_FREQ == 0) {
        calculate_load_avg();
        recalculate_recent_cpu();
    }
    if (get_mlfqs() && timer_ticks() % 4 == 0) {
        recalculate_priorities();
    }
    thread_tick();
    // Interrupts should be disabled during an ISR
    // this will only be called in a timer interrupt
    int64_t ticks_now = timer_ticks();
    threads_wake(ticks_now);
    // if (ticks_now % 10 == 0) printf("\n tick time is %i\n", ticks_now);
}

/* Recalculates the load average */
static void calculate_load_avg(void) {
    // Setting the weight of the old value to be 59/60
    fp old_val_weight = int_to_fp(59);
    old_val_weight = int_divide(old_val_weight, 60);
    // Setting the weight of the ready threads to be 1/60
    fp threads_weight = int_to_fp(1);
    threads_weight = int_divide(threads_weight, 60);
    // The number of threads that are running/ready to run
    // not including the idle thread
    int ready_threads = threads_ready();
    load_avg = fp_add(fp_multiply(old_val_weight, load_avg),
                      int_multiply(threads_weight, ready_threads));
}

/* Recalculates the recent cpu of all threads */
static void recalculate_recent_cpu(void) {
    struct list *all_threads = get_all_list();
    struct list_elem *i = list_front(all_threads);
    for (; i != list_tail(all_threads); i = i->next) {
        fp coef;
        coef = int_multiply(load_avg, 2);
        coef = fp_divide(coef, int_add(int_multiply(load_avg, 2), 1));
        struct thread *t = list_entry(i, struct thread, allelem);
        t->recent_cpu = fp_multiply(coef, t->recent_cpu);
        t->recent_cpu = int_add(t->recent_cpu, t->nice);
    }
}

/* Recalculates the priority of all threads */
static void recalculate_priorities(void) {
    struct list *all_threads = get_all_list();
    struct list_elem *i = list_front(all_threads);
    for (; i != list_tail(all_threads); i = i->next) {
        struct thread *t = list_entry(i, struct thread, allelem);
        thread_calculate_priority(t);
    }
}


/*! Returns true if LOOPS iterations waits for more than one timer tick,
    otherwise false. */
static bool too_many_loops(unsigned loops) {
    /* Wait for a timer tick. */
    int64_t start = ticks;
    while (ticks == start)
        barrier();

    /* Run LOOPS loops. */
    start = ticks;
    busy_wait(loops);

    /* If the tick count changed, we iterated too long. */
    barrier();
    return start != ticks;
}

/*! Iterates through a simple loop LOOPS times, for implementing brief delays.

    Marked NO_INLINE because code alignment can significantly affect timings,
    so that if this function was inlined differently in different places the
    results would be difficult to predict. */
static void NO_INLINE busy_wait(int64_t loops) {
    while (loops-- > 0)
        barrier();
}

/*! Sleep for approximately NUM/DENOM seconds. */
static void real_time_sleep(int64_t num, int32_t denom) {
    /* Convert NUM/DENOM seconds into timer ticks, rounding down.

          (NUM / DENOM) s
       ---------------------- = NUM * TIMER_FREQ / DENOM ticks.
       1 s / TIMER_FREQ ticks
    */
    int64_t ticks = num * TIMER_FREQ / denom;

    ASSERT(intr_get_level() == INTR_ON);
    if (ticks > 0) {
        /* We're waiting for at least one full timer tick.  Use timer_sleep()
           because it will yield the CPU to other processes. */
        timer_sleep(ticks);
    }
    else {
        /* Otherwise, use a busy-wait loop for more accurate sub-tick timing. */
        real_time_delay(num, denom);
    }
}

/*! Busy-wait for approximately NUM/DENOM seconds. */
static void real_time_delay(int64_t num, int32_t denom) {
    /* Scale the numerator and denominator down by 1000 to avoid
       the possibility of overflow. */
    ASSERT(denom % 1000 == 0);
    busy_wait(loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000));
}

