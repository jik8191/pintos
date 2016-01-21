#ifndef BUFFER_H
#define BUFFER_H

typedef struct Buffer {
    unsigned char *array; // The array of keyboard strokes
    int head; // The index of the head
    int tail; // The index of the tail
    int len;  // Index of last element
} buffer;


void init_buffer(buffer *b, unsigned char *array, int len);
unsigned char dequeue(buffer *b);
void enqueue(buffer *b, unsigned char code);
unsigned char peek(buffer *b);

#endif // BUFFER_H
