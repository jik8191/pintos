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

struct lock file_lock;

bool debug_mode = false;

/* TODO when they want us to verify a user pointer do we dereference and
 * then verify? */

static void syscall_handler(struct intr_frame *);

/* Checking pointer validity */
static bool valid_pointer(void **pointer, int size);

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
int sys_open(const char *file);
int sys_read(int fd, void *buffer, unsigned size);
int sys_write(int fd, const void *buffer, unsigned size);

/* Helper functions */
struct file *get_file(int fd);

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
        case SYS_OPEN:
            f->eax = sys_open(to_cchar_p(arg0));
            break;
        case SYS_READ:
            f->eax = sys_read(to_int(arg0), to_void_p(arg1), to_unsigned(arg2));
            break;
        case SYS_WRITE:
            f->eax = sys_write(to_int(arg0), to_cvoid_p(arg1), to_unsigned(arg2));
            break;
        default:
            printf("Call: %d Went to default\n", call_number);
            thread_exit();
    }
}

/* Returns true if addr to addr + size is valid */
bool valid_pointer(void **pointer, int size) {
    void *addr = *pointer;
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
    return * (int *) addr;
}

/* Gets a const char * pointer from the given address. Terminates the process
 * if the pointer is invalid */
static const char *to_cchar_p(void *addr) {
    /* Check if the pointer is invalid */
    if (!valid_pointer(addr, sizeof(const char *))) {
        thread_exit();
    }
    /* Return the pointer */
    else {
        return * (const char **) addr;
    }
}

/* Gets an unsigned starting from the given address. */
static unsigned to_unsigned(void *addr) {
    return * (unsigned *) addr;
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
    struct list *threads = get_all_list();
    struct list_elem *e = list_begin(threads);
    /* Going through the fd's */
    for (; e != list_end(threads); e = list_next(e)) {
        struct thread *t = list_entry(e, struct thread, allelem);
        if (t->pid == pid) {
            return process_wait(t->tid);
        }
    }
    return -1;
}

/* Creating a file with an initial size */
bool sys_create(const char *file, unsigned initial_size) {
    bool return_value;
    lock_acquire(&file_lock);
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
    struct file *file = get_file(fd);
    if (file == NULL) {
        size = 0;
    }
    else {
        size = file_length(file);
    }
    lock_release(&file_lock);
    return size;

}

struct fd_elem {
    struct list_elem elem;
    int fd;
    struct file *file_struct;
};

/* Opens a file */
int sys_open(const char *file) {

    if (debug_mode) {
        printf("In sys_open\n");
        printf("Filename: %s\n", file);
    }

    /* Getting the file struct */
    lock_acquire(&file_lock);
    struct file *file_struct = filesys_open(file);
    lock_release(&file_lock);

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

int sys_read(int fd, void *buffer, unsigned size) {

    if (debug_mode)
        printf("in sys_read\n");

    unsigned bytes_read = 0;
    lock_acquire(&file_lock);

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
        struct file *file = get_file(fd);
        if (file != NULL) {
            bytes_read = file_read(file, buffer, size);
        }
    }
    lock_release(&file_lock);
    return bytes_read;
}

int sys_write(int fd, const void *buffer, unsigned size) {

    if (debug_mode)
        printf("In sys_write\n");

    unsigned bytes_written = 0;

    lock_acquire(&file_lock);

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

    else if (fd == 1) {
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
        struct file *file = get_file(fd);
        if (file != NULL) {
            bytes_written = file_write(file, buffer, size);
        }
        if (bytes_written == 0) {
            if (debug_mode)
                printf("Doesn't seem like anything wrote...");
        }
    }
    lock_release(&file_lock);
    return bytes_written;

}

/* Returns the file struct for a given fd */
struct file *get_file(int fd) {
    struct list_elem *e = list_begin(&thread_current()->fd_list);
    for (; e != list_end(&thread_current()->fd_list); e = list_next(e)) {
        struct fd_elem *curr_fd = list_entry(e, struct fd_elem, elem);
        if (curr_fd->fd == fd) {
            return curr_fd->file_struct;
        }
    }
    return NULL;
}
