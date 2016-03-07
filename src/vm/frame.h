#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include <list.h>
#include <hash.h>

#include "threads/palloc.h"

struct frame {
    void *paddr;    /*!< Address to the page that occupies the frame */
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
struct frame * frame_lookup(void *paddr);

void frame_pin_paddr(void *paddr);
void frame_unpin_paddr(void *paddr);

#endif /* VM_FRAME_H */
