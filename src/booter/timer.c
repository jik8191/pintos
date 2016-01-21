#include "timer.h"
#include "ports.h"
#include "game.h"
#include "handlers.h"

/*============================================================================
 * PROGRAMMABLE INTERVAL TIMER
 *
 * The Programmable Interrupt Timer receives a clock of 1193182 Hz.  It has
 * three frequency-dividers (called "channels", and numbered from 0 to 2),
 * which can divide the clock by any value from 1 to 65535.  This allows one
 * to generate timer ticks for various intervals (approximately).
 *
 * For example, to generate 8 KHz ticks, divide 1193182 / 8000 to get 149.15.
 * Of course, if you put in 149 then you will get an 8008 Hz output, and if
 * you put in 150 then you will get a 7954.5 Hz output.  So, it is clearly an
 * approximate mechanism.
 *
 * Channel 0 is hooked up to IRQ1 on the Programmable Interrupt Controller
 * (PIC), so we can generate timer interrupts on various intervals.
 *
 * Channel 1 is not widely used, and may not even be available on modern
 * hardware.  So don't use it.
 *
 * Channel 2 can be routed to the PC speaker (!!!), so you can generate
 * various tones if you configure PIT channel 2 properly.  Note that this
 * functionality is emulated badly by most CPU emulators, so don't expect
 * that this will necessarily work unless you can test on physical hardware.
 * (See http://wiki.osdev.org/PC_Speaker for more details.)
 *
 * See http://wiki.osdev.org/Programmable_Interval_Timer for more details
 * about the Programmable Interval Timer.
 */

/* Frequency of the PIT's input clock. */
#define PIT_FREQ 1193182

/* Ports for the Programmable Interval Timer (PIT). */
#define PIT_CHAN0_DATA 0x40
#define PIT_CHAN1_DATA 0x41
#define PIT_CHAN2_DATA 0x42
#define PIT_MODE_CMD   0x43

/* How often the timer is firing */
#define FIRE_SPEED 100

/* How often to change the speed of the game */
#define PHASE_LENGTH 15

/* The inital update time */
#define INITIAL_UPDATE 0.25


/* TODO:  You can create static variables here to hold timer state.
 *
 *        You should probably declare variables "volatile" so that the
 *        compiler knows they can be changed by exceptional control flow.
 */

volatile int time_cnt;
volatile float update_time;
int seconds_to_interrupts(float seconds);

void init_timer(void) {

    /* Turn on timer channel 0 for generating interrupts. */
    outb(PIT_MODE_CMD, 0x36);               /* 00 11 011 0 */

    /* Tell channel 0 to trigger 100 times per second.  The value we load
     * here is a divider for the 1193182 Hz timer.  1193182 / 100 ~= 11932.
     * 11932 = 0x2e9c.
     *
     * Always write the low byte first, then high byte second.
     */
    outb(PIT_CHAN0_DATA, 0x9c);
    outb(PIT_CHAN0_DATA, 0x2e);

    /* TODO:  Initialize other timer state here. */
    time_cnt = 0; // Start the time count to be 0
    update_time = INITIAL_UPDATE; // How often to update the game state

    install_interrupt_handler(TIMER_INTERRUPT, irq_timer_handler);
}

void timer_interrupt(void) {
    // A boolean to determine whether to change the game map
    int do_update = time_cnt % seconds_to_interrupts(update_time) == 0;
    // Seeing if you need to update the game state
    //update_game_state(update_map);
    // Increment the time_count
    time_cnt++;
}

int seconds_to_interrupts(float seconds) {
    float num_interrupts;
    num_interrupts = seconds * FIRE_SPEED;
    return (int) num_interrupts;
}
