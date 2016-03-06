#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"
#include <user/syscall.h>

struct lock file_lock;

void syscall_init(void);

void sys_close(int fd);
int sys_open(const char *file);
void sys_munmap(mapid_t mapping);

struct fd_elem {
    struct list_elem elem;
    int fd;
    struct file *file_struct;
};

struct mmap_fileinfo {
    struct list_elem elem;
    mapid_t mapid;
    void *addr;
    int num_pgs;
};

enum conversion_type {
    CONVERT_NUMERIC,
    CONVERT_POINTER
};

#endif /* userprog/syscall.h */

