#include "swap.h"

#include <bitmap.h>
#include <debug.h>
#include <stdio.h>

#include "devices/block.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "vm/frame.h"

#define BLOCKS_PER_PAGE ((int) (PGSIZE) / (BLOCK_SECTOR_SIZE))

/*! Struct representing the swap disk */
struct swapdisk {
    struct block *file;     /*!< Block/file for writing pages to */
    unsigned int size;      /*!< Size of the swap block */
    struct bitmap *slots;   /*!< Bitmap of occupancy. 0 = free, 1 = occupied.
                                 The use of a bitmap requires that the number
                                 of block sectors per page is a whole number,
                                 which it does happen to be. */
};

static struct swapdisk *swap;
static struct lock swaplock;

/*! Initialize the swap data structure and the lock used for swapping. */
void swap_init(void)
{
    swap = malloc(sizeof(struct swapdisk));

    swap->file  = block_get_role(BLOCK_SWAP);
    swap->size  = block_size(swap->file);
    swap->slots = bitmap_create(swap->size);

    lock_init(&swaplock);
}

/*! Swap a page out to the swap file.

    If we've run out of enough slots for the page, we panic. Otherwise, we
    return the block index in the swap file we wrote to. */
block_sector_t swap_page(struct frame *f)
{
    /* printf("Swapping page with user address %x and kernel addr %x\n", f->uaddr, f->kaddr); */
    lock_acquire(&swaplock);

    /* Find a free block large enough for a page */
    block_sector_t idx = bitmap_scan_and_flip(
        swap->slots,        /* The bitmap to search and flip bits inside */
        0,                  /* Start index of the search */
        BLOCKS_PER_PAGE,    /* Number of contiguous blocks we need */
        false               /* We look for false (free) bits and flip to true */
    );

    /* Hopefully you never run out of swap */
    if (idx == BITMAP_ERROR)
        PANIC("You've run out of swap!");

    /* Delta from the initial index */
    int del = 0;

    /* Write the page to swap */
    for (; del < BLOCKS_PER_PAGE; del++) {
        block_write(
            swap->file,
            idx + del,
            f->kaddr + (del * BLOCK_SECTOR_SIZE)
        );
    }

    lock_release(&swaplock);

    /* printf("Swapped to index %d\n", idx); */

    return idx;
}

void swap_load(uint8_t *kaddr, block_sector_t idx)
{
    /* printf("Loading from swap into addr: %x\n", (unsigned) kaddr); */
    lock_acquire(&swaplock);

    frame_pin_kaddr(kaddr);

    /* Delta from the initial index */
    int del = 0;

    /* Read the swap into the specified page */
    for (; del < BLOCKS_PER_PAGE; del++) {
        block_read(
            swap->file,
            idx + del,
            kaddr + (del * BLOCK_SECTOR_SIZE)
        );
    }

    /* Indicate the swap slots are free. */
    bitmap_set_multiple(swap->slots, idx, BLOCKS_PER_PAGE, false);

    /* printf("Swapped from index %d\n", idx); */

    frame_unpin_kaddr(kaddr);

    lock_release(&swaplock);
}


/*! Free swap slots when a thread dies */
void swap_free(block_sector_t idx)
{
    /* Indicate the swap slots are free. */
    bitmap_set_multiple(swap->slots, idx, BLOCKS_PER_PAGE, false);
}
