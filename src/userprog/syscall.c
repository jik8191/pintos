#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "lib/debug.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "lib/user/syscall.h"
#include "userprog/process.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/directory.h"
#include "filesys/inode.h"

bool debug_mode = false;

static void syscall_handler(struct intr_frame *);

/* Checking pointer validity */
static void *valid_pointer(void **pointer, int size);
static void *valid_numeric(void *addr, int size);

/* Conversion functions */
static void *validate_arg(void *addr, enum conversion_type ct, int size);

/* Specific handlers */
void sys_halt(void);
void sys_exit(int status);
pid_t sys_exec(const char *cmd_line);
int sys_wait(pid_t pid);
bool sys_create(const char *file, unsigned initial_size);
bool sys_remove(const char *file);
int sys_filesize(int fd);
int sys_read(int fd, void *buffer, unsigned size);
int sys_write(int fd, const void *buffer, unsigned size);
void sys_seek(int fd, unsigned position);
unsigned sys_tell(int fd);
bool sys_chdir(const char *dir);
bool sys_mkdir(const char *dir);
bool sys_readdir(int fd, char *name);
bool sys_isdir(int fd);
int sys_inumber(int fd);


/* Helper functions */
struct fd_elem *get_file(int fd);

void syscall_init(void)
{
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f)
{

    /* Getting the callers stack pointer */
    void *caller_esp = f->esp;
    int call_number = * (int *) validate_arg(caller_esp, CONVERT_NUMERIC,
                                             sizeof(int));

    /* Getting the args, the calls at most have 3 */
    void *arg0 = f->esp + 4;
    void *arg1 = f->esp + 8;
    void *arg2 = f->esp + 12;

    /* Some variables to put dereferenced args into. */
    int int_arg;
    char *char_arg;
    const char *cchar_arg;
    unsigned unsigned_arg;
    void *void_arg;
    const void *cvoid_arg;

    if (debug_mode) {
        printf("system call!: %d\n", call_number);
    }

    /* Calling the appropriate handler */
    switch (call_number) {
        case SYS_HALT:
            sys_halt();
            break;
        case SYS_EXIT:
            int_arg = * (int *) validate_arg(arg0, CONVERT_NUMERIC,
                                             sizeof(int));
            sys_exit(int_arg);
            break;
        case SYS_EXEC:
            cchar_arg = * (const char **) validate_arg(arg0, CONVERT_POINTER,
                                                       -1);
            f->eax = sys_exec(cchar_arg);
            break;
        case SYS_WAIT:
            int_arg = * (int *) validate_arg(arg0, CONVERT_NUMERIC,
                                             sizeof(int));
            f->eax = sys_wait(int_arg);
            break;
        case SYS_CREATE:
            cchar_arg = * (const char **) validate_arg(arg0, CONVERT_POINTER,
                                                       -1);
            unsigned_arg = * (unsigned *) validate_arg(arg1, CONVERT_NUMERIC,
                                                       sizeof(unsigned));
            f->eax = sys_create(cchar_arg, unsigned_arg);
            break;
        case SYS_REMOVE:
            cchar_arg = * (const char **) validate_arg(arg0, CONVERT_POINTER,
                                                       -1);
            f->eax = sys_remove(cchar_arg);
            break;
        case SYS_FILESIZE:
            int_arg = * (int *) validate_arg(arg0, CONVERT_NUMERIC,
                                             sizeof(int));
            f->eax = sys_filesize(int_arg);
            break;
        case SYS_OPEN:
            cchar_arg = * (const char **) validate_arg(arg0, CONVERT_POINTER,
                                                       -1);
            f->eax = sys_open(cchar_arg);
            break;
        case SYS_READ:
            int_arg = * (int *) validate_arg(arg0, CONVERT_NUMERIC,
                                             sizeof(int));
            unsigned_arg = * (unsigned *) validate_arg(arg2, CONVERT_NUMERIC,
                                                       sizeof(unsigned));
            void_arg = * (void **) validate_arg(arg1, CONVERT_POINTER,
                                                unsigned_arg);
            f->eax = sys_read(int_arg, void_arg, unsigned_arg);
            break;
        case SYS_WRITE:
            int_arg = * (int *) validate_arg(arg0, CONVERT_NUMERIC,
                                             sizeof(int));
            unsigned_arg = * (unsigned *) validate_arg(arg2, CONVERT_NUMERIC,
                                                       sizeof(unsigned));
            cvoid_arg = * (const void **) validate_arg(arg1, CONVERT_POINTER,
                                                       unsigned_arg);
            f->eax = sys_write(int_arg, cvoid_arg, unsigned_arg);
            break;
        case SYS_SEEK:
            int_arg = * (int *) validate_arg(arg0, CONVERT_NUMERIC,
                                             sizeof(int));
            unsigned_arg = * (unsigned *) validate_arg(arg1, CONVERT_NUMERIC,
                                                       sizeof(unsigned));
            sys_seek(int_arg, unsigned_arg);
            break;
        case SYS_TELL:
            int_arg = * (int *) validate_arg(arg0, CONVERT_NUMERIC,
                                             sizeof(int));
            f->eax = sys_tell(int_arg);
            break;
        case SYS_CLOSE:
            int_arg = * (int *) validate_arg(arg0, CONVERT_NUMERIC,
                                             sizeof(int));
            sys_close(int_arg);
            break;
        case SYS_CHDIR:
            cchar_arg = * (const char **) validate_arg(arg0, CONVERT_POINTER,
                                                       -1);
            f->eax = sys_chdir(cchar_arg);
            break;
        case SYS_MKDIR:
            cchar_arg = * (const char **) validate_arg(arg0, CONVERT_POINTER,
                                                       -1);
            f->eax = sys_mkdir(cchar_arg);
            break;
        case SYS_READDIR:
            int_arg = * (int *) validate_arg(arg0, CONVERT_NUMERIC,
                                             sizeof(int));
            char_arg = * (char **) validate_arg(arg1, CONVERT_POINTER,
                                                       -1);
            f->eax = sys_readdir(int_arg, char_arg);
            break;
        case SYS_ISDIR:
            int_arg = * (int *) validate_arg(arg0, CONVERT_NUMERIC,
                                             sizeof(int));
            f->eax = sys_isdir(int_arg);
            break;
        case SYS_INUMBER:
            int_arg = * (int *) validate_arg(arg0, CONVERT_NUMERIC,
                                             sizeof(int));
            f->eax = sys_inumber(int_arg);
            break;
        default:
            printf("Call: %d Went to default\n", call_number);
            sys_exit(-1);
    }
}

/* Returns true if addr to addr + size is valid */
void *valid_pointer(void **pointer, int size)
{
    int i = 0;
    void *kernel_addr = NULL;

    /* Pointer is a pointer to a pointer, first we want to get the actual
     * pointer */
    void *addr = *(pointer);

    /* Need to check that its in user memory */
    if (!is_user_vaddr(addr)) {
        return NULL;
    }

    /* Get the kernel address, if its unmapped return NULL */
    kernel_addr = pagedir_get_page(thread_current()->pagedir, addr);
    if (kernel_addr == NULL) {
        return NULL;
    }

    /* Now to check the rest of the data from the start to size - 1 */
    /* If size is -1 then its for a char * and we want to check memory until
     * we reach a NULL byte */
    if (size == -1) {
        char byte_read;

        /* Check each byte of the char * until we read a null byte */
        i = 1;
        do {
            addr = * (pointer) + i;

            if (!is_user_vaddr(addr)) {
                return NULL;
            }

            if (pagedir_get_page(thread_current()->pagedir, addr) == NULL) {
                return NULL;
            }

            byte_read = * (char *) addr;
            i++;
        } while (byte_read != '\0');


    } else {
        /* Check each byte in the specified byte range. */
        for (i = 1; i < size; i++) {
            addr = *(pointer) + i;

            if (!is_user_vaddr(addr)) {
                return NULL;
            }

            if (pagedir_get_page(thread_current()->pagedir, addr) == NULL) {
                return NULL;
            }
        }
    }
    return kernel_addr;
}

/* Returns true if addr to addr + size is valid */
void *valid_numeric(void *addr, int size)
{
    void *kernel_addr = NULL;

    /* Making sure the memory is in user space */
    if (!is_user_vaddr(addr) || !is_user_vaddr(addr + size - 1)) {
        return NULL;
    }

    /* Getting the kernel virtual address if the address is mapped */
    kernel_addr = pagedir_get_page(thread_current()->pagedir, addr);

    /* Making sure then end of the memory is also mapped */
    if (pagedir_get_page(thread_current()->pagedir, addr + size - 1) == NULL) {
        return NULL;
    }

    return kernel_addr;
}

static void *validate_arg(void *addr, enum conversion_type ct, int size)
{
    void *kernel_addr;

    switch(ct) {

        case(CONVERT_NUMERIC):

            kernel_addr = valid_numeric(addr, size);

            if (kernel_addr == NULL) {
                sys_exit(-1);
            }

            return addr;

        case(CONVERT_POINTER):

            kernel_addr = valid_pointer(addr, size);

            if (kernel_addr == NULL) {
                sys_exit(-1);
            }

            return addr;

        default:
            sys_exit(-1);
    }

    return NULL;
}

void sys_halt(void)
{
    shutdown_power_off();
}

void sys_exit(int status)
{
    /* Have to close all fds */
    if (debug_mode)
        printf("Status: %d\n", status);

    struct thread *t = thread_current();
    t->return_status = status;

    if (t->info != NULL) {
        t->info->return_status = status;
        t->info->terminated = true;
    }

    thread_exit();
}

pid_t sys_exec(const char *cmd_line)
{
    tid_t tid = process_execute(cmd_line);
    if (tid == -1) {
        if (debug_mode)
            printf("Could not create thread\n");
        return -1;
    }

    return tid;
}

int sys_wait(pid_t pid)
{
    return process_wait(pid);
}

/* Creating a file with an initial size */
bool sys_create(const char *file, unsigned initial_size)
{
    if (debug_mode)
        printf("In sys_create\n");
    bool return_value;

    if (file == NULL) {
        sys_exit(-1);
    }
    bool is_dir = false;
    return_value = filesys_create(file, initial_size, is_dir);
    return return_value;
}

/* Removes a file */
bool sys_remove(const char *file)
{
    bool return_value;
    return_value = filesys_remove(file);
    return return_value;
}

/* Gets the size of a file in bytes */
int sys_filesize(int fd)
{
    int size;
    /* Getting the file struct from the fd */
    struct file *file = get_file(fd)->file_struct;
    if (file == NULL) {
        size = 0;
    }
    else {
        size = file_length(file);
    }
    /* lock_release(&file_lock); */
    return size;

}


/* Opens a file */
int sys_open(const char *file)
{

    if (debug_mode) {
        printf("In sys_open\n");
        printf("Filename: %s and thread: %d\n", file, thread_current()->tid);
    }

    /* Getting the file struct */
    if (debug_mode)
        printf("Opening file in thread: %d\n", thread_current()->tid);
    /* printf("file: %s\n", file); */
    struct file *file_struct = filesys_open(file);

    /* If the file can't be opened then put -1 in eax */
    if (file_struct == NULL) {
        if (debug_mode)
            printf("File could not be opened\n");
        return -1;
    }

    if (debug_mode)
        printf("It opened!\n");

    /* Figure out what fd to give the file */
    int fd = thread_current()->max_fd + 1;
    thread_current()->max_fd++;

    struct fd_elem *new_fd = malloc(sizeof(struct fd_elem));
    new_fd->fd = fd;
    new_fd->file_struct = file_struct;
    list_push_back(&thread_current()->fd_list, &new_fd->elem);

    /* Putting the fd in eax */
    if (debug_mode) {
        printf("With fd: %d\n", fd);
    }

    return fd;
}

int sys_read(int fd, void *buffer, unsigned size)
{

    unsigned bytes_read = 0;

    if (debug_mode)
        printf("in sys_read with thread: %d\n", thread_current()->tid);


    if (fd == 0) {
        /* Switching to a char * buffer */
        char *buff = (char *) buffer;

        /* Reading in bytes using input_getc() */
        int i = 0;
        for(; i * sizeof(char) < size; i++) {
            char key = input_getc();
            buff[i] = key;
            bytes_read += sizeof(char);
        }
    }
    else {
        struct fd_elem *file_info= get_file(fd);
        if (file_info != NULL) {
            struct file *file = file_info->file_struct;
            bytes_read = file_read(file, buffer, size);
        }
        else {
            if (debug_mode)
                printf("The file struct was null\n");
            sys_exit(-1);
        }
    }

    return bytes_read;
}

int sys_write(int fd, const void *buffer, unsigned size)
{

    unsigned bytes_written = 0;

    if (debug_mode)
        printf("in sys_write with thread: %d\n", thread_current()->tid);

    if (fd == 1) {
        unsigned i = 0;
        unsigned max_write = 300;
        for (; i < size; i += max_write) {
            if (size - bytes_written > max_write) {
                putbuf(buffer, max_write);
                buffer += max_write;
                bytes_written += max_write;
            }

            else {
                putbuf(buffer, size);
                bytes_written += size;
            }

        }
    }

    else {
        struct fd_elem *file_info = get_file(fd);
        if (file_info != NULL) {
            struct file *file = file_info->file_struct;
            if (inode_is_dir(file->inode)) {
                sys_exit(-1);
            }
            bytes_written = file_write(file, buffer, size);
        }
        else {
            sys_exit(-1);
        }
    }

    return bytes_written;

}

/* Changes the next byte to be read to position */
void sys_seek(int fd, unsigned position)
{
    struct file *file = get_file(fd)->file_struct;
    if (file == NULL) {
        sys_exit(-1);
    }
    file_seek(file, position);
}

/* Returns the position of the next byte to read */
unsigned sys_tell(int fd)
{
    off_t return_value;
    struct file *file = get_file(fd)->file_struct;
    if (file == NULL) {
        sys_exit(-1);
    }
    return_value = file_tell(file);
    return return_value;
}

/* Closes a given file */
void sys_close(int fd)
{
    if (debug_mode)
        printf("In sys close! Closing: %d\n", fd);


    struct fd_elem *file_info = get_file(fd);
    if (file_info == NULL) {
        sys_exit(-1);
    }

    list_remove(&file_info->elem);
    file_close(file_info->file_struct);
    free(file_info);
}

/*! Changes current working directory of process to dir, which may be relative
    or absolute. Return true if successful, false on failure. */
bool sys_chdir(const char *dir)
{

    /* Attempts to get the directory struct */
    struct dir *dir_struct = dir_open_path((char *) dir);

    if (dir_struct != NULL) {
        thread_current()->cwd = dir_struct;
        return true;
    }

    return false;
}


/*! Creates the directory named dir, which may be relative or absolute.
    Returns true if successful, false on failure. Fails if dir already exists
    or if any directory name in dir, besides the last, does not already exist.
    */
bool sys_mkdir(const char *dir)
{
    // mkdir("/a/b/c") succeeds only if "/a/b" exists and "/a/b/c" doesn't.
    bool success;
    bool is_dir = true;
    off_t initial_size = 0;

    success = filesys_create(dir, initial_size, is_dir);
    return success;
}

/*! Reads a directory entry from file descriptor fd, which must represent a
    directory. If successful, stores the null-terminated file name in name,
    which must have room for READDIR_MAX_LEN + 1 bytes, and returns true. If
    no entries are left in the directory, returns false. */
bool sys_readdir(int fd, char *name)
{
    // Do not return "." or ".."
    // If the directory changes while it is open, then it is acceptable for
    // some entries not to be read at all or to be read multiple times.
    // Otherwise, each directory entry should be read once, in any order.

    ASSERT(sys_isdir(fd));
    struct file *file = get_file(fd)->file_struct;
    /* Getting the directory struct */
    /*struct dir *dir = dir_open(file->inode);*/
    return dir_readdir((struct dir *) file, name);
}

/*! Return true if fd represents a directory, false if fd represents an
    ordinary file. */
bool sys_isdir(int fd)
{
    struct file *file = get_file(fd)->file_struct;
    return inode_is_dir(file->inode);
}

/*! Return the inode number of the inode associated with fd, which persistently
    identifies a file or directory and is unique during the file's existence. */
int sys_inumber(int fd)
{
    struct file *file = get_file(fd)->file_struct;

    // the sector number of the inode will be used for the inode number.
    return file->inode->sector;
}


/*! Returns the file struct for a given fd */
struct fd_elem *get_file(int fd)
{
    struct list_elem *e = list_begin(&thread_current()->fd_list);

    for (; e != list_end(&thread_current()->fd_list); e = list_next(e)) {
        struct fd_elem *curr_fd = list_entry(e, struct fd_elem, elem);
        if (curr_fd->fd == fd) {
            return curr_fd;
        }
    }

    return NULL;
}
