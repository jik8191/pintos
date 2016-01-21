#include "buffer.h"
#include "video.h"
#include "keyboard.h"
#include "timer.h"
#include "draw.h"
#include "tunnel.h"

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
    init_tunnel();
    // Clear the screen to the default
    clear_screen();

    /*
    print_tunnels(get_leftwall(), get_rightwall()); // move to start_screen
    print_player(get_playerx(), ROWS - 2);          // move to start_screen
    start_screen();
    */

    /*draw_game();*/
    // Initialize the keyboard
    /*init_keyboard();*/
    // Initialize the timer
    /*init_timer();*/
    int i = 0;
    while (1) {
        draw_game();
    }
}
