#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include "threads/synch.h"

struct bitmap;

/*! In-memory inode. */
struct inode {
    struct list_elem elem;              /*!< Element in inode list. */
    block_sector_t sector;              /*!< Sector number of disk location. */
    int open_cnt;                       /*!< Number of openers. */
    bool removed;                       /*!< True if deleted,
                                             false otherwise. */
    int deny_write_cnt;                 /*!< 0: writes ok, >0: deny writes. */
    struct lock extension_lock;         /*!< Lock for extending file */
};


void inode_init(void);
bool inode_create(block_sector_t, off_t, bool is_dir);
struct inode *inode_open(block_sector_t);
struct inode *inode_reopen(struct inode *);
block_sector_t inode_get_inumber(const struct inode *);
void inode_close(struct inode *);
void inode_remove(struct inode *);
off_t inode_read_at(struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at(struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write(struct inode *);
void inode_allow_write(struct inode *);
off_t inode_length(const struct inode *);
bool inode_is_dir(const struct inode *node);

/* Constants for the number of direct, indirect, and doubly indirect node in
 * the inode_disk structure. */
#define NUM_DIRECT 100
#define NUM_INDIRECT 24
#define NUM_DOUBLE_INDIRECT 1

/* The number of sectors an index block holds */
#define INDEX_BLOCK_SIZE 128

#endif /* filesys/inode.h */
