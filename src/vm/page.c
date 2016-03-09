#include "vm/page.h"

#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hash.h>

#include "threads/malloc.h"
#include "threads/pte.h"
#include "vm/swap.h"

/* Helper for looking up an SPTE */
struct spte * spte_lookup_h(void *vaddr, bool user);

/* Functions for manipulating the supplemental page table (spt) */

/*! Initializes the spt for a thread t*/
bool spt_init (struct thread *t){
    return hash_init(&(t->spt), spte_hash, spte_less, NULL);
}


/*! Returns a hash value for spt entry (spte) p. Code taken from A.8.5 Hash
    Table Example in PintOS docs*/
unsigned spte_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct spte *p = hash_entry (p_, struct spte, hash_elem);
  return hash_bytes (&p->uaddr, sizeof p->uaddr);
}

/*! Look up the page for the given virtual address. Checks for the spte for the
    page using its user virtual address. */
struct spte *spte_lookup(void *vaddr)
{
    /* The supplemental page table for this thread */
    struct hash spt = thread_current()->spt;

    struct spte s;
    struct hash_elem *e;

    /* Need to round down the virtual address to the closest page size.
     * We can do so by zeroing out the lower bytes. */
    s.uaddr = (void *) ((unsigned long) vaddr & (PTMASK | PDMASK));

    e = hash_find (&spt, &s.hash_elem);

    return e != NULL ? hash_entry (e, struct spte, hash_elem) : NULL;
}

/*! Returns true if spte a precedes spte b, the < operator is ok for pointers.
 */
bool spte_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED) {
  const struct spte *a = hash_entry (a_, struct spte, hash_elem);
  const struct spte *b = hash_entry (b_, struct spte, hash_elem);

  return a->uaddr < b->uaddr;
}

/*! Inserts an entry into the spt of thread t so that we know where to load
    program segment data from later on. Return true if successful. */
bool spte_insert (struct thread* t, uint8_t *uaddr, uint8_t *kaddr,
                  struct file *file, off_t ofs,
                  uint32_t read_bytes, uint32_t zero_bytes,
                  enum page_type type, bool writable){
    struct spte *entry;
    entry = malloc(sizeof(struct spte));
    if (entry == NULL)
        return false;

    entry->uaddr      = (void *) uaddr;
    entry->kaddr      = (void *) kaddr;
    entry->file       = file;
    entry->ofs        = ofs;
    entry->read_bytes = read_bytes;
    entry->zero_bytes = zero_bytes;
    entry->writable   = writable;
    entry->type       = type;
    entry->swap_index = NOTSWAPPED;
    entry->loaded     = type == PTYPE_STACK ? true : false;

    // insert entry into thread t's supplemental page table
    return (hash_insert(&t->spt, &entry->hash_elem) == NULL);
}

/*! Removes an entry from the spt of thread t. Return true if successful. */
bool spte_remove (struct thread* t, struct spte *entry){
    struct hash_elem* removed = hash_delete(&(t->spt), &(entry->hash_elem));
    free(entry);
    return (removed != NULL);
}


/*! hash_action_func for spt_destroy, spt_remove. */
void spte_delete(struct hash_elem *e, void *thr UNUSED){
    struct spte *entry = hash_entry(e, struct spte, hash_elem);

    /*! Free swap slots when a thread dies */
    if (entry->swap_index >= 0) {
        swap_free(entry->swap_index);
    }

    free(entry);
}

/*! Destroys the spt for a thread t on exit. */
void spt_destroy (struct thread *t){
    hash_destroy(&(t->spt), *spte_delete);
}
