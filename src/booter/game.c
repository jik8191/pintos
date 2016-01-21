#include "game.h"

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
    init_interrupts();
    init_video();
    init_state();
    init_keyboard();
    init_timer();
    // Clear the screen to the default
    clear_screen();

    /* mask_interrupts(); */
    enable_interrupts();

    draw_game();

    while (1) {
        /* draw_game(); */
    }
}
