#include "frame.h"

#include "threads/thread.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "vm/page.h"

/* ----- Declarations ----- */

struct frame * frame_evict(void);
void frame_replace(struct frame *f);

/* The hash used for the frame table, along with the required hash functions. */
static struct hash frametable;
bool frame_less(const struct hash_elem *, const struct hash_elem *, void *);
unsigned frame_hash(const struct hash_elem *, void *);

static struct list frame_queue;


/* ----- Implementations ----- */

/*! Initialize the data structures needed for managing the page frame
    abstraction. */
void frame_init(void)
{
    hash_init(&frametable, frame_hash, frame_less, NULL);
    list_init(&frame_queue);
}


/*! Return a virtual page and create a frame in the process. */
void * frame_get_page(void *uaddr, enum palloc_flags flags)
{
    void *page = palloc_get_page (flags);

    if (page == NULL) {
        struct frame *evicted = frame_evict();

        /* Free the page and remove the frame for the evicted frame */
        palloc_free_page(evicted->paddr);
        list_remove(&evicted->lelem);
        free(evicted);

        /* Allocate a new page now, which should work */
        page = palloc_get_page (flags);

        ASSERT (page != NULL);
    }

    struct frame *f = malloc (sizeof (struct frame));
    f->paddr  = page;
    f->uaddr  = uaddr;
    f->pinned = false;
    f->dirty  = false;
    f->owner  = thread_current();

    /* Insert it into the frame table */
    hash_insert(&frametable, &f->elem);
    list_push_back(&frame_queue, &f->lelem);

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
        struct list_elem *e = list_begin(&frame_queue);

        for (; e != list_end(&frame_queue); e = list_next(e)) {
            struct frame *f = list_entry(e, struct frame, lelem);

            /* The page directory of the owner of the frame's contents */
            uint32_t *pagedir = f->owner->pagedir;

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

                /* Else if we haven't gotten a new page, we can replace this
                   page with the new one */
                frame_replace(f);
                toswap = f;

            }
        }
    }

    return toswap;
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
    struct spte *page = spte_lookup(f->uaddr);

    /* Whether we can deallocate without swapping */
    bool noswap = true;

    /* Read-only pages don't have to be written to swap */
    if (page->writable) {
        bool dirty;

        /* We check whether the frame has ever been dirty */
        dirty = pagedir_is_dirty(f->owner->pagedir, f->uaddr);
        dirty = dirty || f->dirty; // Set in second-chance eviction.

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
    if (noswap) return;
}

/*! Look up a frame by the address of the page occupying it.

    If a frame exists with the specified page address in the frame table, we
    return the address of the frame. Otherwise, we return NULL, suggesting that
    the specified page is not in a physical page frame at the moment. */
struct frame * frame_lookup(void *paddr)
{
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
    void *aux UNUSED)
{

    struct frame *aframe = hash_entry(a, struct frame, elem);
    struct frame *bframe = hash_entry(b, struct frame, elem);

    return aframe->paddr < bframe->paddr;
}

/*! The function used to hash two frames */
unsigned frame_hash(const struct hash_elem *e, void *aux UNUSED)
{
    struct frame *f = hash_entry(e, struct frame, elem);
    return hash_bytes (&f->paddr, sizeof (f->paddr));
}
