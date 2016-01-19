#include "video.h"

/* This is the address of the VGA text-mode video buffer.  Note that this
 * buffer actually holds 8 pages of text, but only the first page (page 0)
 * will be displayed.
 *
 * Individual characters in text-mode VGA are represented as two adjacent
 * bytes:
 *     Byte 0 = the character value
 *     Byte 1 = the color of the character:  the high nibble is the background
 *              color, and the low nibble is the foreground color
 *
 * See http://wiki.osdev.org/Printing_to_Screen for more details.
 *
 * Also, if you decide to use a graphical video mode, the active video buffer
 * may reside at another address, and the data will definitely be in another
 * format.  It's a complicated topic.  If you are really intent on learning
 * more about this topic, go to http://wiki.osdev.org/Main_Page and look at
 * the VGA links in the "Video" section.
 */
#define VIDEO_BUFFER ((void *) 0xB8000)


/* TODO:  You can create static variables here to hold video display state,
 *        such as the current foreground and background color, a cursor
 *        position, or any other details you might want to keep track of!
 */

static int default_background;
static int default_foreground;
int make_color(int background, int foreground);
void print_char(int x, int y, char c, int color);


void init_video(void) {
    /* TODO:  Do any video display initialization you might want to do, such
     *        as clearing the screen, initializing static variable state, etc.
     */
    default_background = BLUE;
    default_foreground = WHITE;
}

void clear_screen(void) {
    /* Clear the screen and set it to the default background color. */
    int i = 0;
    /* Pointer to the video buffer */
    volatile char *video = (volatile char*) VIDEO_BUFFER;
    for (i = 0; i < WIDTH * HEIGHT; i++) {
        *video++ = ' '; // Setting the char value to a space
        // Setting the color
        *video++ = make_color(default_background, default_foreground); 
    }
    /*print_char(WIDTH - 1, HEIGHT - 1, 'a', make_color(GREEN, RED));*/
    /*print_char(0, 0, 'a', make_color(GREEN, RED));*/
}

int make_color(int background, int foreground) {
    /* Makes a color given the background and foreground */
    int color;
    color = background;
    color = color << 4; // Setting the high nibble
    color = color | foreground; // Setting the low nibble
    return color;
}

void print_char(int x, int y, char c, int color) {
    /* Print a char to a location on the screen with a given color. The screen
     * layout is such that the top left corner is (0, 0) and the bottom left 
     * corner is (WIDTH - 1, HEIGHT - 1)
     */
    volatile char *video = (volatile char*) VIDEO_BUFFER;
    int index = ((y * WIDTH) + x) * 2;
    *(video + index) = c;
    *(video + index + 1) = color;
}

void print_screen(int x, int y, const char *string) {
    /* Prints a string to the screen starting from the given coordinates and
     * goes laterally. 
     */
    while(*string != '\0') {
        print_char(x++, y, *string++, 
                   make_color(default_background, default_foreground));
        if (x == WIDTH) {
            x = 0;
            y++;
        }
    }
}
