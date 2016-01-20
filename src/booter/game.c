#include "buffer.h"
#include "video.h"
#include "keyboard.h"
#include "timer.h"

/* This is the entry-point for the game! */
void c_start(void) {
    /* TODO:  You will need to initialize various subsystems here.  This
     *        would include the interrupt handling mechanism, and the various
     *        systems that use interrupts.  Once this is done, you can call
     *        enable_interrupts() to start interrupt handling, and go on to
     *        do whatever else you decide to do!
     */

    /* Loop forever, so that we don't fall back into the bootloader code. */
    // Initialize the video
    init_video();
    // Clear the screen to the default
    clear_screen();
    /*print_screen(0, 0, "Hello World");*/
    // The keyboard buffer
    buffer *b;
    // Initialize the keyboard
    init_keyboard(b);
    // Initialize the timer
    /*init_timer();*/
    int i = 0;
    while (1) {
        i++;
        if (i % 2000 == 0) {
            enqueue(b, 0x04);
            unsigned char scan_code = dequeue(b);
            if (scan_code == 0x04) {
                /*clear_screen();*/
            }
        }
        else {
            print_screen(0, 0, "Hello World");
        }
    }
}
