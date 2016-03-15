#include "filesys/cache.h"

#include <string.h>

#include "devices/block.h"
#include "threads/synch.h"
#include "filesys/filesys.h"

#define CACHE_SIZE (64)

/*! The actual cache that contains data. */
static struct cache_entry cache[CACHE_SIZE];

/*! Used to control access to the cache. */
static struct lock cache_lock;

/*! Used for the clock-algorithm eviction. */
static int clock_idx;

/* Private function declarations. */
struct cache_entry * cache_get (block_sector_t sector, bool dirty);
struct cache_entry * cache_lookup (block_sector_t sector);
struct cache_entry * cache_new_entry (void);
struct cache_entry * cache_evict (void);
void cache_dump (int cache_index);

/*! Initialize the data structures needed for the file system cache. */
void cache_init (void)
{
    /* "Zero" out the cache. */
    int i;
    for (i = 0; i < CACHE_SIZE; i++) {
        cache[i].valid = false;
        cache[i].dirty = false;
        cache[i].accessed = false;
    }

    lock_init(&cache_lock);

    clock_idx = 0;
}

/*! Flush the entire cache back to disk. */
void cache_flush (void)
{
    lock_acquire(&cache_lock);

    int i;
    for (i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].valid) cache_dump(i);
    }

    lock_release(&cache_lock);
}

/*! Looks up a disk sector in the cache and returns the entry address.

    Returns NULL if the sector is not in the cache. */
struct cache_entry * cache_lookup (block_sector_t sector)
{
    int i;

    /* Find a valid cache entry that represents the sector. */
    for (i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].valid && cache[i].sector == sector)
            return &cache[i];
    }

    return NULL;
}

/*! Read a block sector from cache into a buffer. If it is not in the cache,
    load it into the cache from disk, evicting a cache entry if necessary. */
void cache_read (block_sector_t sector, void *buf)
{
    cache_read_chunk(sector, 0, buf, BLOCK_SECTOR_SIZE);
}

/*! Read a chunk of a block sector from cache into a buffer. If it is not in
    the cache, load it into the cache from disk, evicting a cache entry if
    necessary. */
void cache_read_chunk (block_sector_t sector, off_t sector_ofs, void *buf,
        int chunk_size)
{
    lock_acquire(&cache_lock);

    struct cache_entry *entry = cache_get(sector, false);
    memcpy (buf, entry->data + sector_ofs, chunk_size);

    /* TODO: Read-ahead */

    lock_release(&cache_lock);
}

/*! Write a buffer into a block sector in the cache. If it is not in the cache,
    load it into the cache from disk, evicting a cache entry if necessary. */
void cache_write (block_sector_t sector, const void *buf)
{
    cache_write_chunk(sector, 0, buf, BLOCK_SECTOR_SIZE);
}

/*! Write from a buffer into a chunk of a block sector in cache. If it is not
    in the cache, load it into the cache from disk, evicting a cache entry if
    necessary. */
void cache_write_chunk (block_sector_t sector, off_t sector_ofs,
        const void *buf, int chunk_size)
{
    lock_acquire(&cache_lock);

    struct cache_entry *entry = cache_get(sector, true);
    memcpy (entry->data + sector_ofs, buf, chunk_size);

    /* TODO: Read-ahead */

    lock_release(&cache_lock);
}

/*! Fetches the cache entry corresponding to some sector on disk. If it is not
    in the cache, we add it to the cache.

    Returns the cache entry. */
struct cache_entry * cache_get (block_sector_t sector, bool dirty)
{
    struct cache_entry *entry = cache_lookup(sector);

    if (entry == NULL) {
        entry = cache_new_entry();

        block_read (fs_device, sector, entry->data);

        entry->valid = true;
        entry->sector = sector;
    }

    entry->accessed = true;
    entry->dirty = entry->dirty || dirty;

    return entry;
}

/*! Get an unused entry in the cache we can fill, evicting if necessary. */
struct cache_entry * cache_new_entry (void)
{
    /* Look for an empty slot first. */
    int i;
    for (i = 0; i < CACHE_SIZE; i++) {
        if (!cache[i].valid) return &cache[i];
    }

    /* If all cache entries are full, we need to evict. */
    return cache_evict();
}

/*! Evict a cache entry using the clock algorithm. */
struct cache_entry * cache_evict (void)
{
    while (true) {
        /* Increment the clock hand, wrapping at CACHE_SIZE. We do this first
           since the last time we evicted, we didn't increment after evicting,
           so we want to start at the next index. */
        clock_idx = (clock_idx + 1) % CACHE_SIZE;

        /* We found one to evict. */
        if (!cache[clock_idx].accessed)
            break;

        /* Give a second chance to accessed cache entries. */
        cache[clock_idx].accessed = false;
    }

    /* If the cache entry to evict is dirty, write it back to disk. */
    if (cache[clock_idx].dirty)
        cache_dump(clock_idx);

    /* Make it clean. */
    cache[clock_idx].valid = false;

    return &cache[clock_idx];
}

/*! Dump the data in the `cache_index` slot of the cache back to disk. */
void cache_dump (int cache_index)
{
    /* We don't really care if it's not dirty. */
    if (cache[cache_index].dirty) {
        block_write (
            fs_device,
            cache[cache_index].sector,
            cache[cache_index].data
        );

        cache[cache_index].dirty = false;
    }
}
