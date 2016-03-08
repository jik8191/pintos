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

/* struct frame * frame_evict(void); */
void frame_evict(void);
void frame_replace(struct frame *f);


/* The hash used for the frame table, along with the required hash functions. */
static struct list framequeue;
static struct lock framelock;
struct frame * frame_qlookup(void *vaddr, bool user);


/* ----- Implementations ----- */

/*! Initialize the data structures needed for managing the page frame
    abstraction. */
void frame_init(void)
{
    list_init(&framequeue);
    lock_init(&framelock);
    lock_init(&evictlock);
}

/*! Return a virtual page and create a frame in the process.

    NOTE: The frame is returned pinned. To unpin it, you must call
        frame_unpin(frame_get_page(...))
    afterwards. */
struct frame * frame_get_page(void *uaddr, enum palloc_flags flags)
{
    lock_acquire(&framelock);

    void *page = palloc_get_page (flags);

    if (page == NULL) {
        /* struct frame *evicted = frame_evict(); */
        frame_evict();

        /* Free the page and remove the frame for the evicted frame */
        /* palloc_free_page(evicted->kaddr); */
        /* list_remove(&evicted->lelem); */
        /* free(evicted); */

        /* Allocate a new page now, which should work */
        page = palloc_get_page (flags);

        ASSERT (page != NULL);
    }

    struct frame *f = malloc (sizeof (struct frame));
    /* frame_pin(f); */

    f->kaddr  = page;
    f->uaddr  = uaddr;
    f->dirty  = false;
    f->owner  = thread_current();
    f->pinned = true;

    /* Insert it into the frame table */
    list_push_back(&framequeue, &f->lelem);

    lock_release(&framelock);

    /* return page; */
    return f;
}

void frame_clean(struct thread *t)
{
    lock_acquire(&framelock);

    struct list_elem *e, *prev;
    struct frame *f;

    for (e = list_begin(&framequeue); e != list_end(&framequeue);) {
        f = list_entry(e, struct frame, lelem);

        prev = e;
        e = list_next(e);

        if (f->owner == t) {
            list_remove(prev);
            free(f);
        }
    }

    lock_release(&framelock);
}

/*! Evict a page to swap.

    The eviction policy is the second chance policy. If a frame has been used
    (accessed or dirtied) since our last run through the pages, we will simply
    reset those bits to 0. However, since it is worse to write dirty pages to
    swap than just accessed ones, the accessed bit is set to 0 first, then the
    dirty bit can be reset to 0 in a subsequent pass. The first frame we find
    with neither bit set is the frame whose page we evict. */
void frame_evict(void)
{
    /* Address of frame to evict */
    /* struct frame *toswap = NULL; */
    /* bool swapped = false; */

    /* Keep looping until we actually evict a page */
    while (true) {
        struct list_elem *e = list_pop_front(&framequeue);
        struct frame *f = list_entry(e, struct frame, lelem);

        /* Skip pinned pages */
        if (f->pinned) continue;

        /* The page directory of the owner of the frame's contents */
        uint32_t *pagedir = f->owner->pagedir;

        /* printf("Considering eviction of frame with owner: %x with name: %s from current thread: %s\n", f->owner, f->owner->name, thread_current()->name); */
        /* printf("thread: %x pagedir: %x uaddr: %x\n", */
        /*         (unsigned) f->owner, */
        /*         (unsigned) pagedir, */
        /*         (unsigned) f->uaddr); */

        /* TODO: look at both physical and virtual address for aliasing */
        /* sema_down(&f->owner->pd_sema); */
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

        } else {
            /* Frame the pin so we don't try to replace it twice. We don't
                have to unpin because it will be freed later. */
            frame_pin(f);

            /* Else if we haven't gotten a new page, we can replace this
                page with the new one */
            frame_replace(f);
            /* toswap = f; */
            /* swapped = true; */

            /* sema_up(&f->owner->pd_sema); */
            break;
        }

        /* sema_up(&f->owner->pd_sema); */

        list_push_back(&framequeue, e);
    }

    /* return toswap; */
}

/*! Pin a frame */
void frame_pin(struct frame *f)
{
    f->pinned = true;
}

void frame_pin_kaddr(void *kaddr)
{
    /* lock_acquire(&framelock); */

    struct frame *f = frame_qlookup(kaddr, false);
    ASSERT(f != NULL);
    /* if (f != NULL) */
    frame_pin(f);

    /* lock_release(&framelock); */
}

void frame_pin_uaddr(void *uaddr)
{
    /* lock_acquire(&framelock); */

    struct frame *f = frame_qlookup(uaddr, true);
    ASSERT(f != NULL);
    /* if (f != NULL) */
    frame_pin(f);

    /* lock_release(&framelock); */
}

/*! Unpin a frame */
void frame_unpin(struct frame *f)
{
    f->pinned = false;
}

void frame_unpin_kaddr(void *kaddr)
{
    /* lock_acquire(&framelock); */

    struct frame *f = frame_qlookup(kaddr, false);
    /* ASSERT(f != NULL); */
    if (f != NULL)
        frame_unpin(f);

    /* lock_release(&framelock); */
}

void frame_unpin_uaddr(void *uaddr)
{
    /* lock_acquire(&framelock); */

    struct frame *f = frame_qlookup(uaddr, true);
    /* ASSERT(f != NULL); */
    if (f != NULL)
        frame_unpin(f);

    /* lock_release(&framelock); */
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
    bool locked = false;
    if (!lock_held_by_current_thread(&framelock)) {
        lock_acquire(&framelock);
        locked = true;
    }

    lock_acquire(&evictlock);

    printf("Replacing frame with user addr %x and kernel addr %x\n", f->uaddr, f->kaddr);
    struct spte *page = spte_lookup(f->uaddr);

    /* Whether we can deallocate without swapping */
    bool noswap = true;

    /* Read-only pages don't have to be written to swap */
    /* printf("Page with addr: %x\n", page); */
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
                /* noswap = !dirty; */
                noswap = false;
                break;

            /* We have to write all stack pages */
            case PTYPE_STACK:
                noswap = false;
                break;

            default:
                break;
        }
    }

    /* pagedir_clear_page(f->owner->pagedir, f->uaddr); */

    // We can quit here if we don't have to swap.
    if (noswap) {
        pagedir_clear_page(f->owner->pagedir, f->uaddr);
        goto done;
    }

    /* install_page(f->uaddr, f->kaddr, page->writable); */

    switch (page->type) {
        /* Write these out to the swap file */
        case PTYPE_STACK:
        case PTYPE_CODE:
        case PTYPE_DATA:
            pagedir_clear_page(f->owner->pagedir, f->uaddr);
            page->swap_index = swap_page(f);

            break;

        /* Write these out to the files they belong to */
        case PTYPE_MMAP:
            lock_acquire(&file_lock);

            pagedir_clear_page(f->owner->pagedir, f->uaddr);
            /* Write out the file */
            file_write_at(page->file, f->kaddr, page->read_bytes, page->ofs);

            lock_release(&file_lock);
            break;
    }

    /* pagedir_clear_page(thread_current()->pagedir, f->uaddr); */

done:
    frame_free(f);

    page->loaded = false;

    lock_release(&evictlock);
    if (locked) lock_release(&framelock);
}

void frame_free(struct frame *f)
{
    palloc_free_page(f->kaddr);
    list_remove(&f->lelem);
    free(f);
}
