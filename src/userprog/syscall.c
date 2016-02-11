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

/* TODO accessing files is a critical section. Add locks on calls to anything
 * in filesys and do it for process_execute as well. */

struct lock file_lock;

/* TODO when they want us to verify a user pointer do we dereference and
 * then verify? */

static void syscall_handler(struct intr_frame *);
static bool valid_pointer(void **pointer, int size);
static int addr_to_int(void *addr);
void sys_halt(void);
void sys_exit(void *arg1, struct intr_frame *f);
void sys_create(void *arg1, void *arg2, struct intr_frame *f);
void sys_open(void *arg1, struct intr_frame *f);
void sys_read(void *arg1, void *arg2, void *arg3, struct intr_frame *f);
void sys_write(void *arg1, void *arg2, void *arg3, struct intr_frame *f);
/* NOTE
 * is_user_vaddr returns whether the address is in the user memory from
 * vaddr.h
 */

void syscall_init(void) {
    lock_init(&file_lock);
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f) {
    /* Getting the callers stack pointer */
    void *caller_esp = f->esp;
    int call_number = addr_to_int(caller_esp);
    void *arg1 = f->esp + 4;
    void *arg2 = f->esp + 8;
    void *arg3 = f->esp + 12;
    /*printf("system call!: %d\n", call_number);*/
    switch (call_number) {
        case SYS_HALT:
            sys_halt();
            break;
        case SYS_EXIT:
            sys_exit(arg1, f);
            break;
        case SYS_CREATE:
            sys_create(arg1, arg2, f);
            break;
        case SYS_OPEN:
            sys_open(arg1, f);
            break;
        case SYS_READ:
            sys_read(arg1, arg2, arg3, f);
            break;
        case SYS_WRITE:
            sys_write(arg1, arg2, arg3, f);
            break;
        default:
            printf("Went to default\n");
            thread_exit();
    }
    /* TODO get the syscall number and the arguments */
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

/* Gets the integer starting from the given address. Assumes address is
 * valid.*/
static int addr_to_int(void *addr) {
    return * (int *) addr;
}

/* Gets the pointer starting from the given address */
static void *addr_to_pointer(void *addr) {
    if (!valid_pointer(addr, 4)) {
        return NULL;
    }
    return addr;
}

void sys_halt(void) {
    shutdown_power_off();
}

void sys_exit(void *arg1, struct intr_frame *f) {
    int status = addr_to_int(arg1);
    printf("Status: %d\n", status);
    f->eax = status;
    thread_exit();
}

void sys_create(void *arg1, void *arg2, struct intr_frame *f) {
    if (!valid_pointer(arg1, 4)) {
        printf("Invalid pointer\n");
        thread_exit();
    }

    const char *name = * (const char **) arg1;
    unsigned size = * (unsigned *) arg2;

    bool result = filesys_create(name, size);
    f->eax = result;
}

/* Opens a file */
void sys_open(void *arg1, struct intr_frame *f) {

    printf("In sys_open\n");

    /* Checking to make sure the argument is valid */
    if (!valid_pointer(arg1, 4)) {
        printf("Invalid pointer\n");
        thread_exit();
        /* TODO need to signal to the handler */
        return;
    }

    /* Casting to get the name of the file */
    const char *name = * (const char **) arg1;
    printf("Filename: %s\n", name);

    /* Getting the file struct */
    lock_acquire(&file_lock);
    struct file *file = filesys_open(name);
    lock_release(&file_lock);
    /* If the file can't be opened then put -1 in eax */
    if (file == NULL) {
        printf("File could not be opened\n");
        f->eax = -1;
        return;
    }
    else {
        printf("It opened!\n");
    }

    /* TODO how to get file descripter from the file */
    int fd = (int) file;
    ASSERT(fd != 0);
    ASSERT(fd != 1);

    /* Putting the fd in eax */
    f->eax = fd;
}

void sys_read(void *arg1, void *arg2, void *arg3, struct intr_frame *f) {

    /*printf("in sys_read\n");*/

    if (!valid_pointer(arg2, 4)) {
        printf("Invalid pointer\n");
        thread_exit();
    }

    int fd = * (int *) arg1;;
    unsigned size = * (unsigned *) arg3;

    unsigned bytes_read = 0;

    if (fd == 0) {
        char *buffer = * (char **) arg2;
        while (bytes_read < size) {
            char key = input_getc();
            buffer[bytes_read] = key;
            /* Assuming the size is one byte */
            bytes_read += 1;
        }
        f->eax = bytes_read;
        return;
    }
    else {
        /* TODO for general files */
    }
}

void sys_write(void *arg1, void *arg2, void *arg3, struct intr_frame *f) {

    /*printf("In sys_write\n");*/

    /* Checking the arguments then casting */
    if (!valid_pointer(arg2, 4)) {
        printf("Invalid pointer\n");
        thread_exit();
        /* TODO signal the handler */
        return;
    }

    int fd = *(int *) arg1;
    const void *buffer = * (void **) arg2;
    unsigned size = * (unsigned *) arg3;
    unsigned bytes_written = 0;

    lock_acquire(&file_lock);
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

        f->eax = bytes_written;
    }

    else if (fd == 0) {
        const char *buffer = * (void **) arg2;
        unsigned i = 0;
        enum intr_level old_level = intr_disable();
        for (; i * sizeof(char) < size; i++) {
            input_putc(buffer[i]);
            bytes_written += sizeof(char);
        }
        intr_set_level(old_level);

        f->eax = bytes_written;
    }

    else {
        struct file *file = (struct file *) fd;
        if (file == NULL) {
            printf("Could not get file\n");
        }
        f->eax = file_write(file, buffer, size);

    }
    lock_release(&file_lock);

}
