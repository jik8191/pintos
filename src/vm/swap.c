#include "swap.h"

#include <bitmap.h>
#include <debug.h>

#include "devices/block.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"

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
    lock_acquire(&swaplock);

    /* Find a free block large enough for a page */
    block_sector_t idx = bitmap_scan_and_flip(
        swap->slots,        /* The bitmap to search and flip bits inside */
        0,                  /* Start index of the search */
        BLOCKS_PER_PAGE,    /* Number of contiguous blocks we need */
        false               /* We look for free (false) bits and flip to true */
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
            f->paddr + (del * BLOCK_SECTOR_SIZE)
        );
    }

    lock_release(&swaplock);

    return idx;
}

