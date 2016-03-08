#include "frame.h"

#include <hash.h>

#include "devices/block.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "filesys/file.h"

/* ----- Declarations ----- */

struct frame * frame_evict(void);
void frame_replace(struct frame *f);

void frame_pin(struct frame *f);
void frame_unpin(struct frame *f);

/* The hash used for the frame table, along with the required hash functions. */
static struct list framequeue;
struct frame * frame_qlookup(void *vaddr, bool user);


/* ----- Implementations ----- */

/*! Initialize the data structures needed for managing the page frame
    abstraction. */
void frame_init(void)
{
    list_init(&framequeue);
}

/*! Return a virtual page and create a frame in the process. */
void * frame_get_page(void *uaddr, enum palloc_flags flags)
{
    void *page = palloc_get_page (flags);

    if (page == NULL) {
        struct frame *evicted = frame_evict();

        /* Free the page and remove the frame for the evicted frame */
        palloc_free_page(evicted->kaddr);
        list_remove(&evicted->lelem);
        free(evicted);

        /* Allocate a new page now, which should work */
        page = palloc_get_page (flags);

        ASSERT (page != NULL);
    }

    struct frame *f = malloc (sizeof (struct frame));
    frame_pin(f);

    f->kaddr = page;
    f->uaddr = uaddr;
    f->dirty = false;
    f->owner = thread_current();

    /* Insert it into the frame table */
    list_push_back(&framequeue, &f->lelem);

    frame_unpin(f);

    return page;
}

/*! Evict a page to swap.

    The eviction policy is the second chance policy. If a frame has been used
    (accessed or dirtied) since our last run through the pages, we will simply
    reset those bits to 0. However, since it is worse to write dirty pages to
    swap than just accessed ones, the accessed bit is set to 0 first, then the
    dirty bit can be reset to 0 in a subsequent pass. The first frame we find
    with neither bit set is the frame whose page we evict. */
struct frame * frame_evict(void)
{
    /* Address of frame to evict */
    struct frame *toswap = NULL;

    /* Keep looping until we actually evict a page */
    while (toswap == NULL) {
        struct list_elem *e = list_pop_front(&framequeue);
        struct frame *f = list_entry(e, struct frame, lelem);

        /* Skip pinned pages */
        if (f->pinned) continue;

        /* The page directory of the owner of the frame's contents */
        uint32_t *pagedir = f->owner->pagedir;

        printf("Considering eviction of frame with owner: %x with name: %s from current thread: %s\n", f->owner, f->owner->name, thread_current()->name);
        printf("thread: %x pagedir: %x uaddr: %x\n",
                (unsigned) f->owner,
                (unsigned) pagedir,
                (unsigned) f->uaddr);

        /* TODO: look at both physical and virtual address for aliasing */
        bool accessed = pagedir_is_accessed(pagedir, f->uaddr);
        bool dirty    = pagedir_is_dirty(pagedir, f->uaddr);

        if (accessed) {

            /* If accessed bit is set, remove it */
            pagedir_set_accessed(pagedir, f->uaddr, false);

        } else if (dirty) {

            /* Else if only dirty bit is set, remove it */
            pagedir_set_dirty(pagedir, f->uaddr, false);

            /* Mark that this frame was at one point dirty */
            f->dirty = true;

        } else if (toswap == NULL) {
            /* Frame the pin so we don't try to replace it twice. We don't
                have to unpin because it will be freed later. */
            frame_pin(f);

            /* Else if we haven't gotten a new page, we can replace this
                page with the new one */
            frame_replace(f);
            toswap = f;

            break;
        }

        list_push_back(&framequeue, e);
    }

    return toswap;
}

/*! Pin a frame */
void frame_pin(struct frame *f)
{
    f->pinned = true;
}

void frame_pin_kaddr(void *kaddr)
{
    struct frame *f = frame_qlookup(kaddr, false);
    ASSERT(f != NULL);
    frame_pin(f);
}

void frame_pin_uaddr(void *uaddr)
{
    struct frame *f = frame_qlookup(uaddr, true);
    ASSERT(f != NULL);
    frame_pin(f);
}

/*! Unpin a frame */
void frame_unpin(struct frame *f)
{
    f->pinned = false;
}

void frame_unpin_kaddr(void *kaddr)
{
    struct frame *f = frame_qlookup(kaddr, false);
    ASSERT(f != NULL);
    frame_unpin(f);
}

void frame_unpin_uaddr(void *uaddr)
{
    struct frame *f = frame_qlookup(uaddr, true);
    ASSERT(f != NULL);
    frame_unpin(f);
}

/*! Look up a frame by the address of the page occupying it.

    If a frame exists with the specified page address in the frame table, we
    return the address of the frame. Otherwise, we return NULL, suggesting that
    the specified page is not in a physical page frame at the moment. */
struct frame * frame_qlookup(void *vaddr, bool user)
{
    struct list_elem *e = list_begin(&framequeue);

    for (; e != list_end(&framequeue); e = list_next(e)) {
        struct frame *f = list_entry(e, struct frame, lelem);

        if ((user && f->uaddr == vaddr) || (!user && f->kaddr == vaddr)) {
            return f;
        }
    }

    return NULL;
}

/*! Evict the frame by clearing it out or swapping its contents.

    We can deallocate (without swapping) the following types of pages:
        - Read-only pages
        - Clean data pages
        - Clean mmap-ed pages

    We write the following types of pages to swap:
        - Stack pages
        - Dirty data pages

    We write dirty mmap-ed pages to their file. */
void frame_replace(struct frame *f)
{
    printf("Replacing frame with user addr %x and kernel addr %x\n", f->uaddr, f->kaddr);
    struct spte *page = spte_lookup(f->uaddr);

    /* Whether we can deallocate without swapping */
    bool noswap = true;

    /* Read-only pages don't have to be written to swap */
    if (page->writable) {
        bool dirty;

        /* We check whether the frame has ever been dirty */
        dirty = f->dirty; /* f->dirty set in second-chance eviction. */
        dirty = dirty || pagedir_is_dirty(f->owner->pagedir, f->uaddr);
        dirty = dirty || pagedir_is_dirty(f->owner->pagedir, f->kaddr);

        switch (page->type) {
            /* We only have to swap if the page is dirty */
            case PTYPE_CODE:
            case PTYPE_DATA:
            case PTYPE_MMAP:
                noswap = !dirty;
                break;

            /* We have to write all stack pages */
            case PTYPE_STACK:
                noswap = false;
                break;

            default:
                break;
        }
    }

    // We can quit here if we don't have to swap.
    if (noswap) goto done;

    switch (page->type) {
        /* Write these out to the swap file */
        case PTYPE_STACK:
        case PTYPE_CODE:
        case PTYPE_DATA:
            page->swap_index = swap_page(f);
            break;

        /* Write these out to the files they belong to */
        case PTYPE_MMAP:
            lock_acquire(&file_lock);

            /* Write out the file */
            file_write_at(page->file, f->kaddr, page->read_bytes, page->ofs);

            lock_release(&file_lock);
            break;
    }

done:
    page->loaded = false;
    pagedir_clear_page(f->owner->pagedir, f->uaddr);
}
