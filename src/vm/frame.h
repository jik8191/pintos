#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include <list.h>

#include "threads/palloc.h"

struct frame {
    void *kaddr;    /*!< Kernal address to the page that occupies the frame */
    void *uaddr;    /*!< User virtual addres for the page */
    bool pinned;    /*!< Whether the frame is pinned so it can't be swapped */

    struct thread *owner; /*!< The thread that owns the contents of the frame */

    bool dirty; /*!< This bit is set so we know if the frame contents have been
                     dirtied since its creation. We need this information since
                     we reset the actual dirty bit in our eviction policy */


    struct hash_elem elem;  /*!< Element used to put frame in frame table. */
    struct list_elem lelem; /*!< Element used to put frame in frame queue. */
};

void frame_init(void);
void * frame_get_page(void *uaddr, enum palloc_flags flags);

/* Lookup a frame by its kernel address */
struct frame * frame_lookup(void *kaddr);

/* Pin and unpin a frame by the kernel address of the page occupying it */
void frame_pin_kaddr(void *kaddr);
void frame_unpin_kaddr(void *kaddr);

/* Pin and unpin a frame by the user address of the page occupying it */
void frame_pin_uaddr(void *uaddr);
void frame_unpin_uaddr(void *uaddr);

#endif /* VM_FRAME_H */
