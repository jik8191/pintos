#include "keyboard.h"
#include "video.h"
#include "handlers.h"
#include "random.h"
#include "timer.h"

/* This is the IO port of the PS/2 controller, where the keyboard's scan
 * codes are made available.  Scan codes can be read as follows:
 *
 *     unsigned char scan_code = inb(KEYBOARD_PORT);
 *
 * Most keys generate a scan-code when they are pressed, and a second scan-
 * code when the same key is released.  For such keys, the only difference
 * between the "pressed" and "released" scan-codes is that the top bit is
 * cleared in the "pressed" scan-code, and it is set in the "released" scan-
 * code.
 *
 * A few keys generate two scan-codes when they are pressed, and then two
 * more scan-codes when they are released.  For example, the arrow keys (the
 * ones that aren't part of the numeric keypad) will usually generate two
 * scan-codes for press or release.  In these cases, the keyboard controller
 * fires two interrupts, so you don't have to do anything special - the
 * interrupt handler will receive each byte in a separate invocation of the
 * handler.
 *
 * See http://wiki.osdev.org/PS/2_Keyboard for details.
 */
#define KEYBOARD_PORT 0x60
#define BUFFER_LEN 20


static unsigned char keyboard_array[BUFFER_LEN];
static buffer *keyboard_buffer;

void check_key();

/* Code to set up the keybaord. */
void init_keyboard() {
    // Initialize the keyboard buffer and install the handler
    init_buffer(keyboard_buffer, keyboard_array, BUFFER_LEN);
    install_interrupt_handler(KEYBOARD_INTERRUPT, irq_keyboard_handler);
}

/* What to do when a keyboard interupt fires. */
void keyboard_interrupt(void) {
    // Get the key that was pressed
    unsigned char scan_code = inb(KEYBOARD_PORT);

    // Add it to the pressed queue
    enqueue(keyboard_buffer, scan_code);
    check_key();
}

/* Check the key that was pressed and act accordingly */
void check_key() {
    // Peek into the buffer
    unsigned char scan_code = peek(keyboard_buffer);
    // Get the gamestate
    gamestate state = get_state();
    switch(scan_code) {
        // A is pressed
        case 0x1E:
            if (state == running) {
                // Move the player to the left
                update_player(-1);
            }
            break;

        // D is pressed
        case 0x20:
            if (state == running) {
                // Move the player to the right
                update_player(1);
            }
            break;

        // Space is pressed
        case 0x39:
            if (state == start) {
                // If its in the start state then set the seed and go into
                // the running state
                seed(get_t());
                set_state(running);
            } else if (state == over) {
                // If its in the over state then reset and go back to start
                // state
                reset_t();
                clear_screen();
                init_state();
                set_state(start);
            }

            break;
        // Dequeue from the buffer
        dequeue(keyboard_buffer);
    }
}
