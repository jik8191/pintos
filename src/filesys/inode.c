#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/cache.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/*! Identifies an inode. */
#define INODE_MAGIC 0x494e4f44


/* Functions for single indirect nodes */
static bool is_single_indirect(off_t pos);
static block_sector_t indirect_node_index(off_t pos);
static block_sector_t indirect_pos_index(off_t pos);
static block_sector_t get_indirect(const struct inode *inode, off_t pos);

/* Functions for double indirect nodes */
static bool is_double_indirect(off_t pos);
static block_sector_t double_node_index(off_t pos);
static block_sector_t double_node_second(off_t pos);
static block_sector_t double_pos_index(off_t pos);

/*! On-disk inode.
    Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk {
    off_t length;                       /*!< File size in bytes. */

    /* Arrays to hold the pointers to the locations of the sectors where
     * the file is located. */
    block_sector_t direct[NUM_DIRECT];
    block_sector_t indirect[NUM_INDIRECT];
    block_sector_t double_indirect[NUM_DOUBLE_INDIRECT];

    bool is_dir;                        /*!< Whether file is a directory. */
    unsigned magic;                     /*!< Magic number. */
};

/* Getting an inodes disk */
struct inode_disk *read_disk(const struct inode *inode);

/* Index Block. Holds indicies of sectors that can be other
 * index blocks or actual data. */
struct index_block {
    /* A list of sectors of size index_block_size, currently defined to
     * just be the size of the block sector. */
    block_sector_t sectors[INDEX_BLOCK_SIZE]; /*!< Array of sectors. */
};

/*! Returns the number of sectors to allocate for an inode SIZE
    bytes long. */
static inline size_t bytes_to_sectors(off_t size) {
    return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE);
}

/* Returns true if the index is for a single indirect node */
static bool is_single_indirect(off_t pos) {
    return pos < NUM_DIRECT + (NUM_INDIRECT * INDEX_BLOCK_SIZE);
}

/* Returns the sector of a direct node */
static block_sector_t get_direct(const struct inode *inode, off_t pos) {
    struct inode_disk *disk = read_disk(inode);
    block_sector_t result = disk->direct[pos];
    free(disk);
    return result;
}

/* Gets the index for the single indirect node given pos */
static block_sector_t indirect_node_index(off_t pos) {
    return (pos - NUM_DIRECT) / INDEX_BLOCK_SIZE;
}

/* Gets the actual index of the data for an indirect node given pos */
static block_sector_t indirect_pos_index(off_t pos) {
    return (pos - NUM_DIRECT) % INDEX_BLOCK_SIZE;
}

/* Returns the sector of an indirect node */
static block_sector_t get_indirect(const struct inode *inode, off_t pos) {
    ASSERT(inode != NULL);

    /* Getting the inodes inode_disk from the cache */
    struct inode_disk *disk = read_disk(inode);

    /* The index in the indrect node list. */
    block_sector_t node_index = indirect_node_index(pos);

    /* The sector the indirect node points to. */
    block_sector_t indirect_node_sector = disk->indirect[node_index];

    /* Can now free the disk. */
    free(disk);

    /* Casting the raw data into an index block */
    /* TODO do we want to malloc for this? */
    struct index_block *indices = malloc(sizeof(struct index_block));

    if (indices == NULL) {
        return -1;
    }

    /* Getting the indirect block */
    cache_read(indirect_node_sector, indices);

    /* Getting the index that contains the sector */
    block_sector_t pos_index = indirect_pos_index(pos);

    /* Getting the final result */
    block_sector_t result = indices->sectors[pos_index];

    /* Freeing the index */
    free(indices);

    return result;
}

/* Returns true if the index is for a double indirect node */
static bool is_double_indirect(off_t pos) {
    return pos < NUM_DIRECT + (NUM_INDIRECT * INDEX_BLOCK_SIZE) +
                 (NUM_DOUBLE_INDIRECT * INDEX_BLOCK_SIZE * INDEX_BLOCK_SIZE);
}

/* Gets the index for the double indirect node given pos */
static block_sector_t double_node_index(off_t pos) {
    return (pos - NUM_DIRECT - (NUM_INDIRECT * INDEX_BLOCK_SIZE))
            / (INDEX_BLOCK_SIZE * INDEX_BLOCK_SIZE);
}

/* Gets the index in the first indirect block for a double indirect pos */
static block_sector_t double_node_second(off_t pos) {
    return (pos - NUM_DIRECT - (NUM_INDIRECT * INDEX_BLOCK_SIZE))
            / INDEX_BLOCK_SIZE;
}

/* Gets the actual index of the data for a double indirect node given pos */
static block_sector_t double_pos_index(off_t pos) {
    return (pos - NUM_DIRECT - (NUM_INDIRECT * INDEX_BLOCK_SIZE))
            % INDEX_BLOCK_SIZE;
}

/* Returns the sector of a doubly indirect node */
static block_sector_t get_double_indirect(const struct inode *inode, off_t pos) {
    ASSERT(inode != NULL);

    /* The sector that the double indirect node points to */
    off_t first_node_index = double_node_index(pos);

    /* Getting the inodes inode_disk from the cache */
    struct inode_disk *disk = read_disk(inode);

    /* Getting the sector of the first indirect block */
    block_sector_t first_sector = disk->double_indirect[first_node_index];

    free(disk);

    /* A struct to hold the indirect block */
    struct index_block *indices = malloc(sizeof(struct index_block));

    if (indices == NULL) {
        return -1;
    }

    /* Getting the first indirect block */
    cache_read(first_sector, indices);

    /* The index containing where the second indirect block is */
    off_t second_node_index = double_node_second(pos);

    /* Getting the second indirect block */
    cache_read(indices->sectors[second_node_index], indices);

    free(indices);

    /* Computing the final index */
    off_t pos_index = double_pos_index(pos);

    /* The final result */
    off_t result = indices->sectors[pos_index];

    return result;

}

/*! Returns the block device sector that contains byte offset POS
    within INODE.
    Returns -1 if INODE does not contain data for a byte at offset
    POS. */
static block_sector_t byte_to_sector(const struct inode *inode, off_t pos) {
    ASSERT(inode != NULL);

    /* It can either be a valid position, an invalid position but you are in
     * the process of extending the file, or its an invalid position according
     * to the length but its in the same sector */
    /* TODO do you need the second check */
    if (pos < inode_length(inode) ||
        lock_held_by_current_thread(&inode->extension_lock) ||
        inode_length(inode) / BLOCK_SECTOR_SIZE == pos / BLOCK_SECTOR_SIZE) {

        /* TODO I realize that all these functions should be taking a data_num
         * rather than a pos. Just semantic though */
        /* The number of the sector of actual data this is */
        off_t index = pos / BLOCK_SECTOR_SIZE;

        /* Get the sector depending on its index. */
        if (index < NUM_DIRECT) {

            return get_direct(inode, index);

        } else if (is_single_indirect(index)) {

            return get_indirect(inode, index);

        } else if (is_double_indirect(index)) {

            return get_double_indirect(inode, index);

        }
    }

    return -1;
}

/*! List of open inodes, so that opening a single inode twice
    returns the same `struct inode'. */
static struct list open_inodes;

/*! Initializes the inode module. */
void inode_init(void) {
    list_init(&open_inodes);
}

/* One sector in a list */
struct sector_elem {
    struct list_elem elem;
    block_sector_t sector;
};

/* Function for adding inodes to inode_disk */
bool inode_add(struct inode_disk *disk_inode, size_t add_count, size_t start);

/*! Initializes an inode with LENGTH bytes of data and
    writes the new inode to sector SECTOR on the file system
    device.
    Returns true if successful.
    Returns false if memory or disk allocation fails. */
bool inode_create(block_sector_t sector, off_t length, bool is_dir) {
    /* TODO free the sectors we started to use if we fail */
    struct inode_disk *disk_inode = NULL;
    bool success = false;

    ASSERT(length >= 0);

    /* If this assertion fails, the inode structure is not exactly
       one sector in size, and you should fix that. */
    ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

    /* Allocating space for the inode_disk */
    disk_inode = calloc(1, sizeof *disk_inode);
    if (disk_inode != NULL) {

        /* The number of sectors the data requires */
        size_t sectors = bytes_to_sectors(length);

        /* Setting inode_disk variables */
        disk_inode->length = length;
        disk_inode->magic = INODE_MAGIC;
        disk_inode->is_dir = is_dir;

        /* A list that holds what sectors have been allocated so that we
         * can flip them back if we fail */
        struct list allocated_sectors;
        list_init(&allocated_sectors);

        struct sector_elem disk_sector;

        /* A variable to store which free sector to use */
        block_sector_t cur_sec;

        if (free_map_allocate(1, &cur_sec)) {

            /* Putting the disk sector in the list of sectors */
            disk_sector.sector = cur_sec;
            list_push_back(&allocated_sectors, &disk_sector.elem);

            /* Adding the right amount of sectors relative to the first
             * index in the direct list. */
            success = inode_add(disk_inode, sectors, 0);
        }

        /* Writing the cache to disk */
        cache_write(sector, disk_inode);
    }

    /* TODO need make all the sectors available on failure */

    /* Freeing the disk struct */
    free(disk_inode);
    return success;
}

/* Adds extend_sectors to inode from sector start. Where start isn't the value
 * of the sector but the start relative to the sectors that inode owns. */
bool inode_add(struct inode_disk *disk_inode, size_t add_count, size_t start) {
    bool success = false;

    /* A variable to store which free sector to use */
    block_sector_t cur_sec;

    /* A list of allocated sectors */
    struct list allocated_sectors;
    list_init(&allocated_sectors);

    /* An index block varialbe for making an indirect block */
    struct index_block *single_block = malloc(sizeof(struct index_block));
    /* Whether we need loaded into the single block */
    bool single_loaded = false;
    /* Whether we have freed the single block */
    bool single_freed = false;

    /* An index block variable for making a double indirect block */
    struct index_block *double_block = malloc(sizeof(struct index_block));
    /* Whether we need loaded into the double block */
    bool double_loaded = false;
    /* Whether we have freed the double block */
    bool double_freed = false;

    /* Looping through and making the new sectors */
    size_t i = start;

    /* The last sector to be made */
    size_t end = start + add_count;

    for (; i < end; i++) {

        /* Getting a new sector for the data */
        if (!free_map_allocate(1, &cur_sec)) {
            return success;
        }

        ASSERT(cur_sec != 0);

        /* Adding the data sector to the list of sectors */
        struct sector_elem data_sector;
        data_sector.sector = cur_sec;
        list_push_back(&allocated_sectors, &data_sector.elem);

        /* The case where its just a direct node. */
        if (i < NUM_DIRECT) {

            /* Just update the direct list to have this sector */
            disk_inode->direct[i] = data_sector.sector;

        }

        /* Now the case where its a single indirect node */
        else if (is_single_indirect(i)) {

            /* The index in the indirect list */
            off_t node_index = indirect_node_index(i);

            /* Index of the position of the data */
            off_t pos_index = indirect_pos_index(i);

            /* This means you need to make a new indirect block */
            if (pos_index == 0) {

                /* Making a new indirect block */
                struct index_block *new_indirect =
                    malloc(sizeof(struct index_block));

                if (new_indirect == NULL) {
                    return success;
                }

                /* Setting the single block to this new block */
                single_block = new_indirect;

                /* Getting a sector for the new indirect block */
                if (!free_map_allocate(1, &cur_sec)) {
                    return success;
                }

                /* Adding this sector to the list of sectors */
                struct sector_elem single_sector;
                single_sector.sector = cur_sec;
                list_push_back(&allocated_sectors, &single_sector.elem);

                /* Setting the sector for the indirect list */
                disk_inode->indirect[node_index] = single_sector.sector;

                /* Block is loaded */
                single_loaded = true;

            }
            else if (single_loaded == false) {

                /* Fetch the old index block */
                cache_read(disk_inode->indirect[node_index], single_block);

                /* Block is loaded */
                single_loaded = true;

            }

            /* Adding to the index block */
            single_block->sectors[pos_index] = data_sector.sector;

            /* When you are writing the last instance of the indirect block */
            if (pos_index == INDEX_BLOCK_SIZE - 1 || i == end - 1) {

                /* Writing the index block to disk */
                cache_write(disk_inode->indirect[node_index],
                            (const void *) single_block);

                /* Freeing the single block */
                free(single_block);
                single_freed = true;

            }

        }
        else if (is_double_indirect(i)) {

            /* This is the case where it is doubly indirect */

            /* Index in the double indirect list */
            off_t first_node_index = double_node_index(i);

            /* Index in the single indirect list */
            off_t second_node_index = double_node_second(i);

            /* The final index */
            off_t pos_index = double_pos_index(i);

            /* If we need to make a new double indirect block */
            if (first_node_index == 0 && second_node_index == 0) {

                /* Making the double block */
                struct index_block *new_double =
                    malloc(sizeof(struct index_block));

                if (new_double == NULL) {
                    return success;
                }

                double_block = new_double;

                if (!free_map_allocate(1, &cur_sec)) {
                    return success;
                }

                /* Adding to the list of allocated sectors */
                struct sector_elem double_sector;
                double_sector.sector = cur_sec;
                list_push_back(&allocated_sectors, &double_sector.elem);

                /* Setting the sector for the double indirect list */
                disk_inode->double_indirect[first_node_index] =
                    double_sector.sector;
                double_loaded = true;

            }
            else if (double_loaded == false) {

                /* Need to load the data from the cache */
                cache_read(disk_inode->double_indirect[first_node_index],
                           double_block);
                double_loaded = true;
                single_loaded = false;
            }

            /* We need to make a new single indirect block for the
             * double indirect block */
            if (second_node_index == 0) {

                /* Making the single block */
                struct index_block *new_indirect =
                    malloc(sizeof(struct index_block));

                if (new_indirect == NULL) {
                    return success;
                }

                single_block = new_indirect;

                /* Getting a sector for the new indirect block */
                if (!free_map_allocate(1, &cur_sec)) {
                    return success;
                }

                /* Adding to the list of allocated sectors */
                struct sector_elem single_sector;
                single_sector.sector = cur_sec;
                list_push_back(&allocated_sectors, &single_sector.elem);

                /* Setting the sector for double indirect block */
                double_block->sectors[second_node_index] = single_sector.sector;
                single_loaded = true;

            }
            else if (single_loaded == false) {

                /* Reading the single block from the cache */
                cache_read(double_block->sectors[second_node_index],
                           single_block);
                single_loaded = true;

            }

            /* Setting the position to the single block */
            single_block->sectors[pos_index] = data_sector.sector;

            /* When you are writing the last sector */
            if (i == end - 1) {

                /* Write the double block to disk */
                cache_write(disk_inode->double_indirect[first_node_index],
                            (const void *) double_block);
                free(double_block);
                double_freed = true;

            }

            /* When its the last sector for an indirect block */
            if (pos_index == INDEX_BLOCK_SIZE - 1 || i == end - 1) {

                /* Write the single block to disk */
                cache_write(double_block->sectors[second_node_index],
                            (const void *) single_block);
                free(single_block);
                single_freed = true;

            }
        }
    }

    /* Freeing the block variables if necessary */
    if (!single_freed) {
        free(single_block);
    }

    if (!double_freed) {
        free(double_block);
    }

    success = true;

    return success;
}

/*! Reads an inode from SECTOR
    and returns a `struct inode' that contains it.
    Returns a null pointer if memory allocation fails. */
struct inode * inode_open(block_sector_t sector) {
    struct list_elem *e;
    struct inode *inode;

    /* Check whether this inode is already open. */
    for (e = list_begin(&open_inodes); e != list_end(&open_inodes);
         e = list_next(e)) {
        inode = list_entry(e, struct inode, elem);
        if (inode->sector == sector) {
            inode_reopen(inode);
            return inode;
        }
    }

    /* Allocate memory. */
    inode = malloc(sizeof *inode);
    if (inode == NULL)
        return NULL;

    /* Initialize. */
    list_push_front(&open_inodes, &inode->elem);
    inode->sector = sector;
    inode->open_cnt = 1;
    inode->deny_write_cnt = 0;
    inode->removed = false;

    lock_init(&inode->extension_lock);

    /*cache_read(inode->sector, &inode->data);*/
    return inode;
}

/*! Reopens and returns INODE. */
struct inode * inode_reopen(struct inode *inode) {
    if (inode != NULL)
        inode->open_cnt++;
    return inode;
}

/*! Returns INODE's inode number. */
block_sector_t inode_get_inumber(const struct inode *inode) {
    return inode->sector;
}

/*! Closes INODE and writes it to disk.
    If this was the last reference to INODE, frees its memory.
    If INODE was also a removed inode, frees its blocks. */
void inode_close(struct inode *inode) {
    /* Ignore null pointer. */
    if (inode == NULL)
        return;

    /* Release resources if this was the last opener. */
    if (--inode->open_cnt == 0) {
        /* Remove from inode list and release lock. */
        list_remove(&inode->elem);

        /* Deallocate blocks if removed. */
        if (inode->removed) {

            size_t sectors = bytes_to_sectors(inode_length(inode));
            size_t i = 0;
            struct inode_disk *disk = read_disk(inode);
            for (; i < sectors; i++) {
                /* Freeing the direct nodes */
                if (i < NUM_DIRECT) {
                    free_map_release(disk->direct[i], 1);
                }
                else if (is_single_indirect(i)) {
                    off_t node_index = indirect_node_index(i);
                    off_t pos_index = indirect_pos_index(i);

                    /* Freeing the actual data */
                    free_map_release(get_indirect(inode, i), 1);

                    /* If its the last index to free, free the actual indirect
                     * sector. */
                    if (pos_index == INDEX_BLOCK_SIZE - 1 || i == sectors - 1) {
                        free_map_release(disk->indirect[node_index], 1);
                    }
                }
                else if (is_double_indirect(i)) {
                    off_t first_node_index = double_node_index(i);
                    off_t second_node_index = double_node_second(i);
                    off_t pos_index = double_pos_index(i);

                    /* Freeing the actual data */
                    free_map_release(get_double_indirect(inode, i), 1);

                    /* If its the last sector for an indirect block */
                    if (pos_index == INDEX_BLOCK_SIZE - 1 || i == sectors - 1) {
                        struct index_block *indices = malloc(sizeof(struct index_block));
                        ASSERT(indices != NULL);
                        cache_read(disk->double_indirect[first_node_index], indices);
                        free_map_release(indices->sectors[second_node_index], 1);
                        free(indices);
                    }

                    /* If its the last index to free, free the double indirect
                     * sector */
                    if (i == sectors - 1) {
                        free_map_release(disk->double_indirect[first_node_index], 1);
                    }
                }
            }
            free(disk);
            free_map_release(inode->sector, 1);
        }
        free(inode);
    }
}

/*! Marks INODE to be deleted when it is closed by the last caller who
    has it open. */
void inode_remove(struct inode *inode) {
    ASSERT(inode != NULL);
    inode->removed = true;
}

/*! Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t inode_read_at(struct inode *inode, void *buffer_, off_t size, off_t offset) {
    uint8_t *buffer = buffer_;
    off_t bytes_read = 0;

    while (size > 0) {
        /* Disk sector to read, starting byte offset within sector. */
        block_sector_t sector_idx = byte_to_sector (inode, offset);

        int sector_ofs = offset % BLOCK_SECTOR_SIZE;

        /* Bytes left in inode, bytes left in sector, lesser of the two. */
        off_t inode_left = inode_length(inode) - offset;
        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;

        if ((int) sector_idx == -1) {
            sector_left = 0;
        }

        int min_left = inode_left < sector_left ? inode_left : sector_left;

        /* Number of bytes to actually copy out of this sector. */
        int chunk_size = size < min_left ? size : min_left;
        if (chunk_size <= 0)
            break;

        cache_read_chunk(
            sector_idx,             /* Sector to read. */
            sector_ofs,             /* Sector offset. */
            buffer + bytes_read,    /* Buffer location to read to. */
            chunk_size              /* How much data to read. */
        );

        /* Advance. */
        size -= chunk_size;
        offset += chunk_size;
        bytes_read += chunk_size;
    }

    return bytes_read;
}

/*! Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
    Returns the number of bytes actually written, which may be
    less than SIZE if end of file is reached or an error occurs.
    (Normally a write at end of file would extend the inode, but
    growth is not yet implemented.) */
off_t inode_write_at(struct inode *inode, const void *buffer_, off_t size, off_t offset) {

    const uint8_t *buffer = buffer_;
    off_t bytes_written = 0;

    if (inode->deny_write_cnt)
        return 0;

    bool extended = false;
    off_t new_size;
    struct inode_disk *disk;

    /* Checking if you need to extend the file */
    volatile off_t file_len = inode_length(inode);
    size_t old_sectors = bytes_to_sectors(file_len);
    size_t new_sectors = bytes_to_sectors(offset + size);
    if (offset >= file_len) {
        /* Getting the lock and then double checking. */
        lock_acquire(&inode->extension_lock);
        file_len = inode_length(inode);
        old_sectors = bytes_to_sectors(file_len);
        if (offset >= file_len) {

            extended = true;

            /* Getting how many new sectors are needed */
            size_t add_count = new_sectors - old_sectors;

            disk = read_disk(inode);

            ASSERT(inode_add(disk, add_count, old_sectors) == true);
            cache_write(inode->sector, disk);
            new_size = offset + size;
        }
        else {
            /* Don't actually need to extend */
            lock_release(&inode->extension_lock);
        }
    }

    while (size > 0) {

        /* Sector to write, starting byte offset within the file. */
        block_sector_t sector_idx = byte_to_sector(inode, offset);
        int sector_ofs = offset % BLOCK_SECTOR_SIZE;

        /* Bytes left in inode, bytes left in sector, lesser of the two. */
        off_t inode_left = inode_length(inode) - offset;
        if (extended) {
            inode_left = new_size - offset;
        }
        int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
        int min_left = inode_left < sector_left ? inode_left : sector_left;

        /* Number of bytes to actually write into this sector. */
        int chunk_size = size < min_left ? size : min_left;
        if (chunk_size <= 0)
            break;

        cache_write_chunk(
            sector_idx,
            sector_ofs,
            buffer + bytes_written,
            chunk_size
        );

        /* Advance. */
        size -= chunk_size;
        offset += chunk_size;
        bytes_written += chunk_size;
    }

    if (extended) {
        /* Update file size and release lock */
        disk->length = new_size;
        /* Writing to disk to cache */
        cache_write(inode->sector, disk);
        lock_release(&inode->extension_lock);
        free(disk);
    }

    return bytes_written;
}

/*! Disables writes to INODE.
    May be called at most once per inode opener. */
void inode_deny_write (struct inode *inode) {
    inode->deny_write_cnt++;
    ASSERT(inode->deny_write_cnt <= inode->open_cnt);
}

/*! Re-enables writes to INODE.
    Must be called once by each inode opener who has called
    inode_deny_write() on the inode, before closing the inode. */
void inode_allow_write (struct inode *inode) {
    ASSERT(inode->deny_write_cnt > 0);
    ASSERT(inode->deny_write_cnt <= inode->open_cnt);
    inode->deny_write_cnt--;
}

/*! Returns the length, in bytes, of INODE's data. */
off_t inode_length(const struct inode *inode) {
    struct inode_disk *disk = read_disk(inode);
    off_t length = disk->length;
    free(disk);
    return length;
}

struct inode_disk *read_disk(const struct inode *inode) {
    /* Getting the inodes inode_disk from the cache */
    struct inode_disk *disk = malloc(sizeof(struct inode_disk));
    ASSERT(disk != NULL);
    cache_read(inode->sector, disk);
    return disk;
}

bool inode_is_dir(const struct inode *inode)
{
    struct inode_disk *disk = malloc(sizeof(struct inode_disk));
    cache_read(inode->sector, disk);
    return disk->is_dir;
}
