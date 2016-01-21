#include "game.h"

#include "buffer.h"
#include "video.h"
#include "keyboard.h"
#include "timer.h"
#include "draw.h"
#include "state.h"

/* This is the entry-point for the game! */
void c_start(void) {

    init_video();
    init_interrupts();
    init_keyboard();
    init_timer();
    init_state();

    // Clear the screen to the default
    clear_screen();
    draw_game();

    enable_interrupts();

    while (1) {
        // Everything is handled by timer interrupts, so this is fine.
    }
}
