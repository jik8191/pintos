#include "buffer.h"
/* Change this code to use an array because thats how we will have to implement
 * it because we do not have malloc.
 */

void init_buffer(buffer *b, unsigned char *array, int len) {
    b->array = array;
    b->head = 0;
    b->tail = 0;
    b->len = len;
}

unsigned char dequeue(buffer *b) {
    // Get the data from the head of the buffer and replace the head
    unsigned char code = b->array[b->head]; // Getting the data
    b->head = b->len % (b->head + 1); // Incrementing the head
    return code;
}

void enqueue(buffer *b, unsigned char code) {
    // Put the next code at the end of the buffer
    int index = b->len % (b->tail + 1);
    b->array[index] = code;
    b->tail = index;
}
