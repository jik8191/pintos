#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "devices/block.h"
#include "vm/frame.h"

void swap_init(void);

block_sector_t swap_page(struct frame *);

#endif /* VM_SWAP_H */
