#include "frame.h"

/* ----- Declarations ----- */

/* The hash used for the frame table, along with the required hash functions. */
static struct hash frametable;
bool frame_less(const struct hash_elem *, const struct hash_elem *, void *);
unsigned frame_hash(const struct hash_elem *, void *);


/* ----- Implementations ----- */

/*! Initialize the data structures needed for managing the page frame
    abstraction. */
void frame_init() {
    hash_init(&frametable, frame_hash, frame_less, NULL);
}

void frame_get_page() {
}


/*! Look up a frame by the address of the page occupying it.

    If a frame exists with the specified page address in the frame table, we
    return the address of the frame. Otherwise, we return NULL, suggesting that
    the specified page is not in a physical page frame at the moment. */
struct frame * frame_lookup(const void *paddr) {
    struct frame f;
    struct hash_elem *e;

    f.paddr = paddr;
    e = hash_find (&frametable, &f.elem);

    return e != NULL ? hash_entry (e, struct frame, elem) : NULL;
}


/* ----- Hash Table Functions ----- */

/*! The function used to compare two frames */
bool frame_less(
    const struct hash_elem *a,
    const struct hash_elem *b,
    void *aux UNUSED) {

    struct frame *aframe = hash_entry(a, struct frame, elem);
    struct frame *bframe = hash_entry(b, struct frame, elem);

    return aframe->paddr < bframe->paddr;
}

/*! The function used to hash two frames */
unsigned frame_hash(const struct hash_elem *e, void *aux UNUSED) {
    struct frame *f = hash_entry(e, struct frame, elem);
    return hash_bytes (&f->addr, sizeof f->addr);
}
