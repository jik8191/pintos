#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"
#include "threads/malloc.h"

#include "threads/thread.h"

/*! Partition that contains the file system. */
struct block *fs_device;

static void do_format(void);

/*! Initializes the file system module.
    If FORMAT is true, reformats the file system. */
void filesys_init(bool format) {
    fs_device = block_get_role(BLOCK_FILESYS);
    if (fs_device == NULL)
        PANIC("No file system device found, can't initialize file system.");

    inode_init();
    free_map_init();

    if (format)
        do_format();

    free_map_open();
}

/*! Shuts down the file system module, writing any unwritten data to disk. */
void filesys_done(void) {
    free_map_close();
    cache_flush();
}

/*! Creates a file named NAME with the given INITIAL_SIZE.  Returns true if
    successful, false otherwise.  Fails if a file named NAME already exists,
    or if internal memory allocation fails. */
bool filesys_create(const char *name, off_t initial_size, bool is_dir) {
    // printf("'path is %s'\n", (const char*) name);
    char **dir_filename = convert_path(name);

    if (dir_filename == NULL) {
        return false;
    }

    /*printf("Directory %s\n", dir_filename[0]);*/
    /*printf("File %s\n", dir_filename[1]);*/

    block_sector_t inode_sector = 0;
    struct dir *dir = dir_open_path(dir_filename[0]);
    if (dir == NULL) {
        printf("The dir was null\n");
    }
    bool success = (dir != NULL &&
                    free_map_allocate(1, &inode_sector) &&
                    inode_create(inode_sector, initial_size, is_dir) &&
                    dir_add(dir, dir_filename[1], inode_sector, is_dir));

    if (!success && inode_sector != 0)
        free_map_release(inode_sector, 1);

    /* Add directory connections. */
    /* if (success && is_dir) { */
    /*     struct dir *newdir = dir_open_path(name); */
    /*  */
    /*     dir_add(newdir, ".", inode_sector); */
    /*     dir_add(newdir, "..", dir->inode->sector); */
    /*  */
    /*     dir_close(newdir); */
    /* } */

    dir_close(dir);

    /*
    if (is_dir) {
        printf("Made directory %s in directory %s\n", dir_filename[1],
            dir_filename[0]);
    }
    else {
        printf("Made file %s in directory %s\n", dir_filename[1], dir_filename[0]);
    }
    */

    free(dir_filename[0]);
    free(dir_filename[1]);
    free(dir_filename);
    return success;
}

/*! Opens the file with the given NAME.  Returns the new file if successful
    or a null pointer otherwise.  Fails if no file named NAME exists,
    or if an internal memory allocation fails. */
struct file * filesys_open(const char *name) {
    struct inode *inode = NULL;
    struct dir *dir;
    char *file = NULL;

    char **dir_filename = convert_path(name);

    /* TODO: This check doesn't work. */
    if (dir_filename[0] == NULL) {
        dir = dir_reopen(thread_current()->cwd);
    }
    else {
        printf("dir: %s, file: %s\n", dir_filename[0], dir_filename[1]);
        dir = dir_open_path(dir_filename[0]);
        file = dir_filename[1];

        /* If there was no file component, we just open the directory. */
        if (file[0] == '\0') {
            free(file);

            file = malloc(2 * sizeof(char));
            file[0] = '.';
            file[1] = '\0';
        }
    }

    dir_lookup(dir, file, &inode);
    dir_close(dir);

    return file_open(inode);
}

/*! Deletes the file named NAME.  Returns true if successful, false on failure.
    Fails if no file named NAME exists, or if an internal memory allocation
    fails. */
bool filesys_remove(const char *name) {
    struct dir *dir;

    char **dir_filename = convert_path(name);

    if (dir_filename[0] == NULL) {
        dir = dir_reopen(thread_current()->cwd);
    } else {
        dir = dir_open_path(dir_filename[0]);
        name = dir_filename[1];
    }

    /* TODO: Check if files remain. */
    /* TODO: Support rm /a/ instead of just rm /a */

    bool success = dir != NULL && dir_remove(dir, name);
    dir_close(dir);

    return success;
}

/*! Formats the file system. */
static void do_format(void) {
    printf("Formatting file system...");
    free_map_create();
    if (!dir_create(ROOT_DIR_SECTOR, 16))
        PANIC("root directory creation failed");
    free_map_close();
    printf("done.\n");
}

