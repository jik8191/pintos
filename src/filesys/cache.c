#include "filesys/cache.h"

#include <debug.h>
#include <string.h>

#include "devices/block.h"
#include "devices/timer.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"

#define CACHE_SIZE (64)
#define FLUSH_INTERVAL (1000)

/*! The actual cache that contains data. */
static struct cache_entry cache[CACHE_SIZE];

/*! Used to control access to the cache. */
static struct lock cache_lock;

/*! Used for the clock-algorithm eviction. */
static int clock_idx;

/*! Used for asynchronous read-ahead. */
struct ra_entry {
    block_sector_t sector;  /*!< The sector to read into cache. */
    struct list_elem elem;  /*!< To be put into the read-ahead queue. */
};
static struct list ra_queue;
static struct lock ra_qlock;
static struct semaphore ra_wait_sema;
void read_ahead_add (block_sector_t sector);

void read_ahead_d (void *);
void write_behind_d (void *);

/* Private function declarations. */
struct cache_entry * cache_get (block_sector_t sector);
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
        cache[i].pinned = false;

        rwlock_init(&cache[i].rw_lock);
    }

    lock_init(&cache_lock);

    clock_idx = 0;

    /* Initialize the data structures needed for asynchronous read-ahead. */
    list_init(&ra_queue);
    lock_init(&ra_qlock);
    sema_init(&ra_wait_sema, 0);

    /* Spin up the asynchronous read-ahead and write-behind jobs. */
    thread_create ("read_ahead_daemon", PRI_DEFAULT, &read_ahead_d, NULL);
    thread_create ("write_behind_daemon", PRI_MAX, &write_behind_d, NULL);
}

/*! Flush the entire cache back to disk. */
void cache_flush (void)
{
    lock_acquire(&cache_lock);

    int i;
    for (i = 0; i < CACHE_SIZE; i++) {
        rwlock_acquire_writer(&cache[i].rw_lock);
        if (cache[i].valid) cache_dump(i);
        rwlock_release_writer(&cache[i].rw_lock);
    }

    lock_release(&cache_lock);
}

/*! Unpin a cache entry with the given sector. */
void cache_unpin_sector (block_sector_t sector)
{
    struct cache_entry *entry = cache_lookup(sector);

    ASSERT (entry != NULL);

    rwlock_acquire_writer(&entry->rw_lock);
    entry->pinned = false;
    rwlock_release_writer(&entry->rw_lock);
}

/*! Returns a pointer to the start of the data in the cache entry. The cache
    entry is pinned before the pointer is returned, so that the data is not
    evicted. You should call cache_unpin_sector when you are done with the
    data. */
void * cache_get_pinned_read_ptr (block_sector_t sector)
{
    struct cache_entry *entry = cache_get(sector);

    rwlock_acquire_writer(&entry->rw_lock);

    /* We released the global lock before acquiring the entry lock, so the
       entry could have changed. We need to check that it didn't, or reload
       if it did. */
    while ((volatile block_sector_t) entry->sector != sector) {
        rwlock_release_writer(&entry->rw_lock);

        entry = cache_get(sector);

        rwlock_acquire_writer(&entry->rw_lock);
    }

    entry->pinned = true;

    rwlock_release_writer(&entry->rw_lock);

    return &entry->data;
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
    struct cache_entry *entry = cache_get(sector);

    rwlock_acquire_reader(&entry->rw_lock);
    entry->pinned = true;

    /* We released the global lock before acquiring the entry lock, so the
       entry could have changed. We need to check that it didn't, or reload
       if it did. */
    while ((volatile block_sector_t) entry->sector != sector) {
        rwlock_release_reader(&entry->rw_lock);

        entry = cache_get(sector);

        rwlock_acquire_reader(&entry->rw_lock);
        entry->pinned = true;
    }

    memcpy (buf, entry->data + sector_ofs, chunk_size);

    entry->pinned = false;
    rwlock_release_reader(&entry->rw_lock);
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
    struct cache_entry *entry = cache_get(sector);

    rwlock_acquire_writer(&entry->rw_lock);
    entry->pinned = true;

    /* We released the global lock before acquiring the entry lock, so the
       entry could have changed. We need to check that it didn't, or reload
       if it did. */
    while ((volatile block_sector_t) entry->sector != sector) {
        rwlock_release_writer(&entry->rw_lock);

        entry = cache_get(sector);

        rwlock_acquire_writer(&entry->rw_lock);
        entry->pinned = true;
    }

    entry->dirty = true;
    memcpy (entry->data + sector_ofs, buf, chunk_size);

    entry->pinned = false;
    rwlock_release_writer(&entry->rw_lock);
}

/*! Fetches the cache entry corresponding to some sector on disk. If it is not
    in the cache, we add it to the cache.

    Returns the cache entry. */
struct cache_entry * cache_get (block_sector_t sector)
{
    lock_acquire(&cache_lock);

    struct cache_entry *entry = cache_lookup(sector);

    if (entry == NULL) {
        entry = cache_new_entry();

        block_read (fs_device, sector, entry->data);

        entry->sector = sector;
        entry->valid = true;
    }

    entry->accessed = true;

    /* Read-ahead one sector. */
    read_ahead_add (sector + 1);

    lock_release(&cache_lock);

    return entry;
}

/*! Add the specified sector to the read_ahead queue. */
void read_ahead_add (block_sector_t sector)
{
    /* Make sure it's actually in our file system */
    if (sector + 1 < block_size(fs_device)) {
        struct ra_entry *ra = malloc(sizeof(struct ra_entry));

        /* If, for some reason, we ran out of memory, we can just skip this
           read-ahead. */
        if (ra == NULL) return;

        ra->sector = sector + 1;

        lock_acquire(&ra_qlock);
        list_push_back(&ra_queue, &ra->elem);
        lock_release(&ra_qlock);

        /* Let the daemon know we have something to read-ahead. */
        sema_up(&ra_wait_sema);
    }
}

/*! Thread that runs and when told that there is something to read ahead of
    time, loads that into the cache. */
void read_ahead_d (void *aux UNUSED)
{
    struct list_elem *e;
    struct ra_entry *ra_entry = NULL;
    struct cache_entry *entry = NULL;

    while (true) {
        /* Wait until we actually have something to pop. */
        sema_down(&ra_wait_sema);

        /* Get the next sector to read. */
        lock_acquire(&ra_qlock);
        e = list_pop_front(&ra_queue);
        lock_release(&ra_qlock);

        ra_entry = list_entry (e, struct ra_entry, elem);

        /* If it's not already in the cache, we want to load it. */
        if (cache_lookup(ra_entry->sector) == NULL) {
            lock_acquire(&cache_lock);

            /* Find a free cache slot and load it. */
            entry = cache_new_entry();

            block_read (fs_device, ra_entry->sector, entry->data);

            entry->sector = ra_entry->sector;
            entry->valid = true;

            lock_release(&cache_lock);
        }

        free(ra_entry);
    }
}

/*! Thread that runs and flushes the cache to disk periodically. */
void write_behind_d (void *aux UNUSED)
{
    while (true) {
        timer_sleep (FLUSH_INTERVAL);
        cache_flush();
    }
}

/*! Looks up a disk sector in the cache and returns the entry address.

    Returns NULL if the sector is not in the cache. */
struct cache_entry * cache_lookup (block_sector_t sector)
{
    int i;

    /* Find a valid cache entry that represents the sector. */
    for (i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].valid && cache[i].sector == sector) {
            cache[i].accessed = true;
            return &cache[i];
        }
    }

    return NULL;
}

/*! Get an unused entry in the cache we can fill, evicting if necessary. */
struct cache_entry * cache_new_entry (void)
{
    ASSERT(lock_held_by_current_thread(&cache_lock));

    /* Look for an empty slot first. */
    int i;
    for (i = 0; i < CACHE_SIZE; i++) {
        if (!cache[i].valid)
            return &cache[i];
    }

    /* If all cache entries are full, we need to evict. */
    return cache_evict();
}

/*! Evict a cache entry using the clock algorithm. */
struct cache_entry * cache_evict (void)
{
    ASSERT(lock_held_by_current_thread(&cache_lock));

    while (true) {
        /* Increment the clock hand, wrapping at CACHE_SIZE. We do this first
            since the last time we evicted, we didn't increment after evicting,
            so we want to start at the next index. */
        clock_idx = (clock_idx + 1) % CACHE_SIZE;

        /* Skip pinned entries. */
        if (cache[clock_idx].pinned) continue;

        /* We found one to evict. */
        if (!cache[clock_idx].accessed) break;

        /* Give a second chance to accessed cache entries. */
        cache[clock_idx].accessed = false;
    }

    rwlock_acquire_writer(&cache[clock_idx].rw_lock);

    cache[clock_idx].pinned = true;

    /* Make it clean. */
    cache[clock_idx].valid = false;

    /* If the cache entry to evict is dirty, write it back to disk. */
    if (cache[clock_idx].dirty)
        cache_dump(clock_idx);

    cache[clock_idx].pinned = false;

    rwlock_release_writer(&cache[clock_idx].rw_lock);

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
