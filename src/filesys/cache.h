#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <stdbool.h>

#include "devices/block.h"
#include "threads/synch.h"
#include "filesys/off_t.h"

/*! An entry in the disk cache. */
struct cache_entry {
    /* Bits used for eviction of cache entries. */
    bool dirty;             /*!< Whether the cache entry has been written to. */
    bool accessed;          /*!< Whether the cache entry has been accessed. */

    block_sector_t sector;  /*!< The disk sector that this buffer is caching. */

    bool valid;             /*!< True if the cache entry is actually holding
                                 any valid data; false if it's just empty. */

    bool pinned;            /*!< The cache entry can't get evicted. */

    struct rwlock rw_lock;  /*!< Read-write lock for controlling access to the
                                 cache entry */

    uint8_t data[BLOCK_SECTOR_SIZE]; /*!< The data from the disk the buffer is
                                          caching. */
};

void cache_init (void);
void cache_flush (void);

void cache_read (block_sector_t, void *);
void cache_read_chunk (block_sector_t, off_t, void *, int);

void cache_write (block_sector_t, const void *);
void cache_write_chunk (block_sector_t, off_t, const void *, int);

void * cache_get_pinned_read_ptr (block_sector_t sector);
void cache_unpin_sector (block_sector_t sector);

#endif /* FILESYS_CACHE_H */

