#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>

#include "threads/palloc.h"

struct frame {
    void *paddr;    /*!< Address to the page that occupies the frame */

    struct hash_elem elem;  /*!< Element used to put frame in frame table. */
};

#endif /* VM_FRAME_H */
