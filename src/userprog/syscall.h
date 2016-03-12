#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"

struct lock file_lock;

void syscall_init(void);

void sys_close(int fd);
int sys_open(const char *file);

struct fd_elem {
    struct list_elem elem;
    int fd;
    struct file *file_struct;
};

#endif /* userprog/syscall.h */

