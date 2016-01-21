#include "keyboard.h"

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


/* TODO:  You can create static variables here to hold keyboard state.
 *        Note that if you create some kind of circular queue (a very good
 *        idea, you should declare it "volatile" so that the compiler knows
 *        that it can be changed by exceptional control flow.
 *
 *        Also, don't forget that interrupts can interrupt *any* code,
 *        including code that fetches key data!  If you are manipulating a
 *        shared data structure that is also manipulated from an interrupt
 *        handler, you might want to disable interrupts while you access it,
 *        so that nothing gets mangled...
 */

// TODO does these need to be volatile?
static unsigned char keyboard_array[BUFFER_LEN];
static buffer *keyboard_buffer;

void check_key();

void init_keyboard() {
    /* TODO:  Initialize any state required by the keyboard handler. */

    /* TODO:  You might want to install your keyboard interrupt handler
     *        here as well.
     */
    install_interrupt_handler(KEYBOARD_INTERRUPT, keyboard_interrupt);
    init_buffer(keyboard_buffer, keyboard_array, BUFFER_LEN);
}

void keyboard_interrupt(void) {
    // Get the key that was pressed
    unsigned char scan_code = inb(KEYBOARD_PORT);
    // Add it to the pressed queue
    disable_interrupts();
    enqueue(keyboard_buffer, scan_code);
    check_key();
    enable_interrupts();
}

void check_key() {
    unsigned char scan_code = peek(keyboard_buffer);
    gamestate state = get_state();
    switch(scan_code) {
        // A is pressed
        case 0x1E:
            if (state == running) {
                update_player(-1);
            }
            break;
        // D is pressed
        case 0x20:
            if (state == running) {
                update_player(1);
            }
            break;
        // Space is pressed
        case 0x39:
            if (state == start) {
                set_state(running);
            }
            break;
    }
}
