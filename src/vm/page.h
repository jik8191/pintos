#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <debug.h>
#include <stdint.h>
#include <hash.h>

#include "devices/block.h"
#include "filesys/file.h"
#include "threads/thread.h"

/*! Used for the swap index for an spte when the data is not swapped */
#define NOTSWAPPED (-1)

enum page_type {
    PTYPE_STACK,
    PTYPE_MMAP,
    PTYPE_CODE,
    PTYPE_DATA
};

/* Supplemental page table entry (spte) struct.
 * Each thread will have its own spte. The entry holds additional
 * data about each page for virtual memory management. */
/* TODO might be helpful to add the physical address of the frame */
struct spte {
    // see load_segment() in process.c for more about how these fields
    // are used
    void *uaddr;               /*!< user page address, used as key. */
    void *kaddr;
    struct file *file;
    off_t ofs;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;
    struct hash_elem hash_elem; /*!< To put spte in spt. */

    enum page_type type;    /*!< Type of user page */
    int swap_index;         /*!< Index of the swapped data, if any */
};

bool spt_init (struct thread *t);

unsigned spte_hash (const struct hash_elem *p_, void *aux UNUSED);
struct spte *spte_lookup(void *vaddr);
bool spte_less (const struct hash_elem *a_, const struct hash_elem *b_,
                void *aux UNUSED);

bool spte_insert (struct thread* t, uint8_t *uaddr, uint8_t *kaddr,
                  struct file *file, off_t ofs,
                  uint32_t read_bytes, uint32_t zero_bytes,
                  enum page_type type, bool writable);

bool spte_remove (struct thread* t, struct spte *entry);

void spte_delete(struct hash_elem *e, void *thr);

void spt_destroy (struct thread *t);

#endif /* vm/page.h */
