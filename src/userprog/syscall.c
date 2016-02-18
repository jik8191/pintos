#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "lib/debug.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "lib/user/syscall.h"
#include "userprog/process.h"
#include "threads/malloc.h"

/* TODO accessing files is a critical section. Add locks on calls to anything
 * in filesys and do it for process_execute as well. */


bool debug_mode = false;

/* TODO when they want us to verify a user pointer do we dereference and
 * then verify? */

static void syscall_handler(struct intr_frame *);

/* Checking pointer validity */
static bool valid_pointer(void **pointer, int size);
static bool valid_numeric(void *addr, int size);

/* Conversion functions */
static int to_int(void *addr);
static const char *to_cchar_p(void *addr);
static unsigned to_unsigned(void *addr);
static const void *to_cvoid_p(void *addr);
static void *to_void_p(void *addr);

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

/* Helper functions */
struct fd_elem *get_file(int fd);

void syscall_init(void) {
    lock_init(&file_lock);
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f) {

    /* Getting the callers stack pointer */
    /* TODO currently assuming that this is valid memory */
    void *caller_esp = f->esp;
    int call_number = to_int(caller_esp);

    /* Getting the args, the calls at most have 3 */
    /* TODO assuming that these addresses are valid */
    void *arg0 = f->esp + 4;
    void *arg1 = f->esp + 8;
    void *arg2 = f->esp + 12;

    if (debug_mode) {
        printf("system call!: %d\n", call_number);
    }

    /* Calling the appropriate handler */
    switch (call_number) {
        case SYS_HALT:
            sys_halt();
            break;
        case SYS_EXIT:
            sys_exit(to_int(arg0));
            break;
        case SYS_EXEC:
            f->eax = sys_exec(to_cchar_p(arg0));
            break;
        case SYS_WAIT:
            f->eax = sys_wait(to_int(arg0));
            break;
        case SYS_CREATE:
            f->eax = sys_create(to_cchar_p(arg0), to_unsigned(arg1));
            break;
        case SYS_REMOVE:
            f->eax = sys_remove(to_cchar_p(arg0));
            break;
        case SYS_FILESIZE:
            f->eax = sys_filesize(to_int(arg0));
            break;
        case SYS_OPEN:
            f->eax = sys_open(to_cchar_p(arg0));
            break;
        case SYS_READ:
            f->eax = sys_read(to_int(arg0), to_void_p(arg1), to_unsigned(arg2));
            break;
        case SYS_WRITE:
            f->eax = sys_write(to_int(arg0), to_cvoid_p(arg1), to_unsigned(arg2));
            break;
        case SYS_SEEK:
            sys_seek(to_int(arg0), to_unsigned(arg1));
            break;
        case SYS_TELL:
            f->eax = sys_tell(to_int(arg0));
            break;
        case SYS_CLOSE:
            sys_close(to_int(arg0));
            break;
        default:
            printf("Call: %d Went to default\n", call_number);
            thread_exit();
    }
}

/* Returns true if addr to addr + size is valid */
bool valid_pointer(void **pointer, int size) {
    /*printf("Pointer %p\n", pointer);*/
    void *addr = *pointer;
    /*printf("Pointer %p\n", addr);*/
    if (!is_user_vaddr(addr) || !is_user_vaddr(addr + size)) {
        return false;
    }
    if (pagedir_get_page(thread_current()->pagedir, addr) == NULL ||
        pagedir_get_page(thread_current()->pagedir, addr + size) == NULL) {
        return false;
    }
    return true;
}

/* Returns true if addr to addr + size is valid */
bool valid_numeric(void *addr, int size) {
    if (!is_user_vaddr(addr) || !is_user_vaddr(addr + size)) {
        return false;
    }
    if (pagedir_get_page(thread_current()->pagedir, addr) == NULL ||
        pagedir_get_page(thread_current()->pagedir, addr + size) == NULL) {
        return false;
    }
    return true;
}


/* Gets the integer starting from the given address. */
static int to_int(void *addr) {
    if (!valid_numeric(addr, sizeof(int))) {
        thread_exit();
    } else {
        return * (int *) addr;
    }
}

/* Gets a const char * pointer from the given address. Terminates the process
 * if the pointer is invalid */
static const char *to_cchar_p(void *addr) {
    /* Check if the pointer is invalid */
    if (!valid_pointer(addr, sizeof(const char *))) {
        thread_exit();
    }
    else {
        return * (const char **) addr;
    }
}

/* Gets an unsigned starting from the given address. */
static unsigned to_unsigned(void *addr) {
    /* Check if the pointer is invalid */
    if (!valid_numeric(addr, sizeof(unsigned))) {
        thread_exit();
    }
    else {
        return * (unsigned *) addr;
    }
}

/* Gets a void pointer from the given address. */
static const void*to_cvoid_p(void *addr) {
    /* Check if the pointer is invalid */
    if (!valid_pointer(addr, sizeof(const void *))) {
        thread_exit();
    }
    return * (const void **) addr;
}

/* Gets a void pointer from the given address. */
static void *to_void_p(void *addr) {
    /* Check if the pointer is invalid */
    if (!valid_pointer(addr, sizeof(void *))) {
        thread_exit();
    }
    return * (void **) addr;
}

void sys_halt(void) {
    shutdown_power_off();
}

void sys_exit(int status) {
    /* TODO says to return the status to the kernel, how to? */
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

pid_t sys_exec(const char *cmd_line) {
    tid_t tid = process_execute(cmd_line);
    if (tid == -1) {
        if (debug_mode)
            printf("Could not create thread\n");
        return -1;
    }
    /* TODO probably need a better way of giving out pid's */
    return tid;
    /*return (int) get_thread(tid);*/
}

int sys_wait(pid_t pid) {
    return process_wait(pid);
}

/* Creating a file with an initial size */
bool sys_create(const char *file, unsigned initial_size) {
    if (debug_mode)
        printf("In sys_create\n");
    bool return_value;
    lock_acquire(&file_lock);
    /* TODO should this be covered before here? */
    if (file == NULL) {
        lock_release(&file_lock);
        thread_exit();
    }
    return_value = filesys_create(file, initial_size);
    lock_release(&file_lock);
    return return_value;
}

/* Removes a file */
bool sys_remove(const char *file) {
    bool return_value;
    lock_acquire(&file_lock);
    return_value = filesys_remove(file);
    lock_release(&file_lock);
    return return_value;
}

/* Gets the size of a file in bytes */
int sys_filesize(int fd) {
    int size;
    /* TODO are we ensured the file is open? */
    lock_acquire(&file_lock);
    /* Getting the file struct from the fd */
    struct file *file = get_file(fd)->file_struct;
    if (file == NULL) {
        size = 0;
    }
    else {
        size = file_length(file);
    }
    lock_release(&file_lock);
    return size;

}


/* Opens a file */
int sys_open(const char *file) {

    if (debug_mode) {
        printf("In sys_open\n");
        printf("Filename: %s and thread: %d\n", file, thread_current()->tid);
    }

    /* Getting the file struct */
    lock_acquire(&file_lock);
    if (debug_mode)
        printf("Opening file in thread: %d\n", thread_current()->tid);
    struct file *file_struct = filesys_open(file);

    /* If the file can't be opened then put -1 in eax */
    if (file_struct == NULL) {
        if (debug_mode)
            printf("File could not be opened\n");
        lock_release(&file_lock);
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
    lock_release(&file_lock);

    /* Putting the fd in eax */
    if (debug_mode) {
        printf("With fd: %d\n", fd);
    }
    return fd;
}

int sys_read(int fd, void *buffer, unsigned size) {

    unsigned bytes_read = 0;
    lock_acquire(&file_lock);

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
            lock_release(&file_lock);
            thread_exit();
        }
    }
    /*printf("About to release lock with thread: %d\n", thread_current()->tid);*/
    lock_release(&file_lock);
    /*printf("And it has been released\n");*/
    return bytes_read;
}

int sys_write(int fd, const void *buffer, unsigned size) {

    unsigned bytes_written = 0;

    lock_acquire(&file_lock);

    if (debug_mode)
        printf("in sys_write with thread: %d\n", thread_current()->tid);

/*
    if (fd == 0) {
        const char *buffer = (const char *) buffer;
        unsigned i = 0;
        enum intr_level old_level = intr_disable();
        for (; i * sizeof(char) < size; i++) {
            input_putc(buffer[i]);
            bytes_written += sizeof(char);
        }
        intr_set_level(old_level);
    }
*/

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
            bytes_written = file_write(file, buffer, size);
        }
        else {
            lock_release(&file_lock);
            thread_exit();
        }
    }
    /*printf("About to release lock with thread: %d\n", thread_current()->tid);*/
    lock_release(&file_lock);
    /*printf("And it has been released\n");*/
    return bytes_written;

}

/* Changes the next byte to be read to position */
void sys_seek(int fd, unsigned position) {
    lock_acquire(&file_lock);
    struct file *file = get_file(fd)->file_struct;
    if (file == NULL) {
        lock_release(&file_lock);
        thread_exit();
    }
    file_seek(file, position);
    lock_release(&file_lock);
}

/* Returns the position of the next byte to read */
unsigned sys_tell(int fd) {
    off_t return_value;
    lock_acquire(&file_lock);
    struct file *file = get_file(fd)->file_struct;
    if (file == NULL) {
        lock_acquire(&file_lock);
        thread_exit();
    }
    return_value = file_tell(file);
    lock_release(&file_lock);
    return return_value;
}

/* Closes a given file */
void sys_close(int fd) {
    if (debug_mode)
        printf("In sys close! Closing: %d\n", fd);

    lock_acquire(&file_lock);

    struct fd_elem *file_info = get_file(fd);
    if (file_info == NULL) {
        lock_release(&file_lock);
        thread_exit();
    }

    list_remove(&file_info->elem);
    file_close(file_info->file_struct);
    free(file_info);

    lock_release(&file_lock);
}

/* Returns the file struct for a given fd */
struct fd_elem *get_file(int fd) {
    struct list_elem *e = list_begin(&thread_current()->fd_list);
    for (; e != list_end(&thread_current()->fd_list); e = list_next(e)) {
        struct fd_elem *curr_fd = list_entry(e, struct fd_elem, elem);
        if (curr_fd->fd == fd) {
            return curr_fd;
        }
    }
    return NULL;
}
