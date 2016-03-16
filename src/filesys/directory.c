#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/*! A directory. */
struct dir {
    struct inode *inode;                /*!< Backing store. */
    off_t pos;                          /*!< Current position. */
};

/*! A single directory entry. */
struct dir_entry {
    block_sector_t inode_sector;        /*!< Sector number of header. */
    char name[NAME_MAX + 1];            /*!< Null terminated file name. */
    bool in_use;                        /*!< In use or free? */
};


/*! Creates a directory with space for ENTRY_CNT entries in the
    given SECTOR.  Returns true if successful, false on failure. */
bool dir_create(block_sector_t sector, size_t entry_cnt) {
    bool is_dir = true;
    return inode_create(sector, entry_cnt * sizeof(struct dir_entry), is_dir);
}

/*! Opens and returns the directory for the given INODE, of which
    it takes ownership.  Returns a null pointer on failure. */
struct dir * dir_open(struct inode *inode) {
    struct dir *dir = calloc(1, sizeof(*dir));
    if (inode != NULL && dir != NULL) {
        dir->inode = inode;
        dir->pos = 0;
        return dir;
    }
    else {
        inode_close(inode);
        free(dir);
        return NULL;
    }
}

/*! Opens the root directory and returns a directory for it.
    Return true if successful, false on failure. */
struct dir * dir_open_root(void) {
    return dir_open(inode_open(ROOT_DIR_SECTOR));
}


/*! Opens the directory specified and returns a dir pointer for it.
 *  Return NULL on failure. */
struct dir * dir_open_path(char *path){
    struct dir *wd;   // working directory

    // make a copy of path
    int len = strlen(path);
    char *s = malloc((len+1)*sizeof(char*)); // +1 for null termination
    memcpy(s, path, sizeof(char)*len);

    // check if we are dealing with an absolute path or NULL cwd
    // start traversing from root directory if so
    struct dir *cwd = thread_current()->cwd;
    if (s[0] == '/' || cwd == NULL) {
        // printf("Opened root directory.\n");
        wd = dir_open_root();
    } else {
        // printf("Opening path %s.\n", path);
        wd = dir_reopen(cwd);
    }

    // tokenize the path and traverse it as we do so
    char *token, *save_ptr;
    struct inode *inode_wd; // inode representing wd as we traverse
    for (token = strtok_r(s, "/", &save_ptr); token != NULL;
         token = strtok_r(NULL, "/", &save_ptr)){
        // TODO: catch this in iteration if possible
        // handle case where string only has '/' and token now 
        // points to null termination string
        if (token-s == len){
            break;
        }
        // search for next file/directory in path, store inode representation
        // in inode_wd
        if(!dir_lookup(wd, token, &inode_wd)){
            return NULL;
        } else {
            dir_close(wd);
            wd = dir_open(inode_wd);
            if (!wd) return NULL;
        }
    }
    free(s);
    return wd;
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir * dir_reopen(struct dir *dir) {
    return dir_open(inode_reopen(dir->inode));
}

/*! Destroys DIR and frees associated resources. */
void dir_close(struct dir *dir) {
    if (dir != NULL) {
        inode_close(dir->inode);
        free(dir);
    }
}

/*! Returns the inode encapsulated by DIR. */
struct inode * dir_get_inode(struct dir *dir) {
    return dir->inode;
}

/*! Searches DIR for a file with the given NAME.
    If successful, returns true, sets *EP to the directory entry
    if EP is non-null, and sets *OFSP to the byte offset of the
    directory entry if OFSP is non-null.
    otherwise, returns false and ignores EP and OFSP. */
static bool lookup(const struct dir *dir, const char *name,
                   struct dir_entry *ep, off_t *ofsp) {
    struct dir_entry e;
    size_t ofs;

    ASSERT(dir != NULL);
    ASSERT(name != NULL);

    for (ofs = 0; inode_read_at(dir->inode, &e, sizeof(e), ofs) == sizeof(e);
         ofs += sizeof(e)) {
        if (e.in_use && !strcmp(name, e.name)) {
            if (ep != NULL)
                *ep = e;
            if (ofsp != NULL)
                *ofsp = ofs;
            return true;
        }
    }
    return false;
}

/*! Searches DIR for a file with the given NAME and returns true if one exists,
    false otherwise.  On success, sets *INODE to an inode for the file,
    otherwise to a null pointer.  The caller must close *INODE. */
bool dir_lookup(const struct dir *dir, const char *name, struct inode **inode) {
    struct dir_entry e;

    ASSERT(dir != NULL);
    ASSERT(name != NULL);

    if (lookup(dir, name, &e, NULL))
        *inode = inode_open(e.inode_sector);
    else
        *inode = NULL;

    return *inode != NULL;
}

/*! Adds a file named NAME to DIR, which must not already contain a file by
    that name.  The file's inode is in sector INODE_SECTOR.
    Returns true if successful, false on failure.
    Fails if NAME is invalid (i.e. too long) or a disk or memory
    error occurs. */
bool dir_add(struct dir *dir, const char *name, block_sector_t inode_sector) {
    struct dir_entry e;
    off_t ofs;
    bool success = false;

    ASSERT(dir != NULL);
    ASSERT(name != NULL);

    /* Check NAME for validity. */
    if (*name == '\0' || strlen(name) > NAME_MAX)
        return false;

    /* Check that NAME is not in use. */
    if (lookup(dir, name, NULL, NULL))
        goto done;

    /* Set OFS to offset of free slot.
       If there are no free slots, then it will be set to the
       current end-of-file.

       inode_read_at() will only return a short read at end of file.
       Otherwise, we'd need to verify that we didn't get a short
       read due to something intermittent such as low memory. */
    for (ofs = 0; inode_read_at(dir->inode, &e, sizeof(e), ofs) == sizeof(e);
         ofs += sizeof(e)) {
        if (!e.in_use)
            break;
    }

    /* Write slot. */
    e.in_use = true;
    strlcpy(e.name, name, sizeof e.name);
    e.inode_sector = inode_sector;
    success = inode_write_at(dir->inode, &e, sizeof(e), ofs) == sizeof(e);

done:
    return success;
}

/*! Removes any entry for NAME in DIR.  Returns true if successful, false on
    failure, which occurs only if there is no file with the given NAME. */
bool dir_remove(struct dir *dir, const char *name) {
    struct dir_entry e;
    struct inode *inode = NULL;
    bool success = false;
    off_t ofs;

    ASSERT(dir != NULL);
    ASSERT(name != NULL);

    /* Find directory entry. */
    if (!lookup(dir, name, &e, &ofs))
        goto done;

    /* Open inode. */
    inode = inode_open(e.inode_sector);
    if (inode == NULL)
        goto done;

    /* Erase directory entry. */
    e.in_use = false;
    if (inode_write_at(dir->inode, &e, sizeof(e), ofs) != sizeof(e))
        goto done;

    /* Remove inode. */
    inode_remove(inode);
    success = true;

done:
    inode_close(inode);
    return success;
}

/*! Reads the next directory entry in DIR and stores the name in NAME.  Returns
    true if successful, false if the directory contains no more entries. */
bool dir_readdir(struct dir *dir, char name[NAME_MAX + 1]) {
    struct dir_entry e;

    while (inode_read_at(dir->inode, &e, sizeof(e), dir->pos) == sizeof(e)) {
        dir->pos += sizeof(e);
        if (e.in_use) {
            strlcpy(name, e.name, NAME_MAX + 1);
            return true;
        }
    }
    return false;
}

/*! Given a path name to a file, turn it into a directory string and 
 *  a filename string. */
char **convert_path (const char *path){
    int len = strlen(path);
    char *token, *save_ptr;
    char *s = malloc((len+1)*sizeof(char)); // +1 for null termination
    memcpy(s, path, sizeof(char)*len); // manipulate the copy
    *(s+len) = '\0';

    // TODO: improve this allocation, reallocate if necessary
    char **tokens = malloc(64*sizeof(char*));
    char *dir = malloc(len*sizeof(char));
    char *dir_ur = dir;  // will not be manipulated
    char *filename = malloc(len*sizeof(char));
    char **dir_filename = malloc(2*sizeof(char*));
    int num_tokens;
    int i = 0;

    // check we we use absolute path
    // TODO: handle cases such as /../
    if (s[0] == '/'){
        *dir = '/';
        dir++;
    }
    
    for (token = strtok_r(s, "/", &save_ptr); token != NULL;
         token = strtok_r(NULL, "/", &save_ptr)){
        tokens[i] = token;
        // printf("length of token '%s' is %d\n", token, strlen(tokens[i]));
        i++;
    }
    num_tokens = i;
    for(i=0; i<num_tokens-1; i++){
        memcpy(dir, tokens[i], sizeof(char)*strlen(tokens[i]));
        dir += strlen(tokens[i]);
        *dir = '/';
        dir++;
    }

    memcpy(filename, tokens[num_tokens-1],
           sizeof(char)*strlen(tokens[num_tokens-1]));
    *(filename+strlen(tokens[num_tokens-1])) = '\0';
    *dir = '\0'; // null-termination
    dir_filename[0] = dir_ur;
    dir_filename[1] = filename;

    // printf("directory is %s\n", dir_filename[0]);
    // printf("filename is %s\n", dir_filename[1]);
    free(tokens);
    free(s);
    return dir_filename;
}

