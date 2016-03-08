#include "userprog/exception.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/pte.h"
#include "userprog/process.h"
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"


/*! Number of page faults processed. */
static long long page_fault_cnt;

static void kill(struct intr_frame *);
static void page_fault(struct intr_frame *);

/*! Registers handlers for interrupts that can be caused by user programs.

    In a real Unix-like OS, most of these interrupts would be passed along to
    the user process in the form of signals, as described in [SV-386] 3-24 and
    3-25, but we don't implement signals.  Instead, we'll make them simply kill
    the user process.

    Page faults are an exception.  Here they are treated the same way as other
    exceptions, but this will need to change to implement virtual memory.

    Refer to [IA32-v3a] section 5.15 "Exception and Interrupt Reference" for a
    description of each of these exceptions. */
void exception_init(void) {
    /* These exceptions can be raised explicitly by a user program,
       e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
       we set DPL==3, meaning that user programs are allowed to
       invoke them via these instructions. */
    intr_register_int(3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
    intr_register_int(4, 3, INTR_ON, kill, "#OF Overflow Exception");
    intr_register_int(5, 3, INTR_ON, kill,
                      "#BR BOUND Range Exceeded Exception");

    /* These exceptions have DPL==0, preventing user processes from
       invoking them via the INT instruction.  They can still be
       caused indirectly, e.g. #DE can be caused by dividing by
       0.  */
    intr_register_int(0, 0, INTR_ON, kill, "#DE Divide Error");
    intr_register_int(1, 0, INTR_ON, kill, "#DB Debug Exception");
    intr_register_int(6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
    intr_register_int(7, 0, INTR_ON, kill,
                      "#NM Device Not Available Exception");
    intr_register_int(11, 0, INTR_ON, kill, "#NP Segment Not Present");
    intr_register_int(12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
    intr_register_int(13, 0, INTR_ON, kill, "#GP General Protection Exception");
    intr_register_int(16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
    intr_register_int(19, 0, INTR_ON, kill,
                      "#XF SIMD Floating-Point Exception");

    /* Most exceptions can be handled with interrupts turned on.
       We need to disable interrupts for page faults because the
       fault address is stored in CR2 and needs to be preserved. */
    intr_register_int(14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/*! Prints exception statistics. */
void exception_print_stats(void) {
    printf("Exception: %lld page faults\n", page_fault_cnt);
}

/*! Handler for an exception (probably) caused by a user process. */
static void kill(struct intr_frame *f) {
    /* This interrupt is one (probably) caused by a user process.
       For example, the process might have tried to access unmapped
       virtual memory (a page fault).  For now, we simply kill the
       user process.  Later, we'll want to handle page faults in
       the kernel.  Real Unix-like operating systems pass most
       exceptions back to the process via signals, but we don't
       implement them. */

    /* The interrupt frame's code segment value tells us where the
       exception originated. */
    switch (f->cs) {
    case SEL_UCSEG:
        /* User's code segment, so it's a user exception, as we
           expected.  Kill the user process.  */
        printf("%s: dying due to interrupt %#04x (%s).\n",
               thread_name(), f->vec_no, intr_name(f->vec_no));
        intr_dump_frame(f);
        thread_exit();

    case SEL_KCSEG:
        /* Kernel's code segment, which indicates a kernel bug.
           Kernel code shouldn't throw exceptions.  (Page faults
           may cause kernel exceptions--but they shouldn't arrive
           here.)  Panic the kernel to make the point.  */
        intr_dump_frame(f);
        PANIC("Kernel bug - unexpected interrupt in kernel");

    default:
        /* Some other code segment?  Shouldn't happen.  Panic the
           kernel. */
        printf("Interrupt %#04x (%s) in unknown segment %04x\n",
               f->vec_no, intr_name(f->vec_no), f->cs);
        thread_exit();
    }
}

/*! Page fault handler.  This is a skeleton that must be filled in
    to implement virtual memory.  Some solutions to project 2 may
    also require modifying this code.

    At entry, the address that faulted is in CR2 (Control Register
    2) and information about the fault, formatted as described in
    the PF_* macros in exception.h, is in F's error_code member.  The
    example code here shows how to parse that information.  You
    can find more information about both of these in the
    description of "Interrupt 14--Page Fault Exception (#PF)" in
    [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void page_fault(struct intr_frame *f) {
    bool not_present;  /* True: not-present page, false: writing r/o page. */
    bool write;        /* True: access was write, false: access was read. */
    bool user;         /* True: access by user, false: access by kernel. */
    void *fault_addr;  /* Fault address. */

    /* Obtain faulting address, the virtual address that was accessed to cause
       the fault.  It may point to code or to data.  It is not necessarily the
       address of the instruction that caused the fault (that's f->eip).
       See [IA32-v2a] "MOV--Move to/from Control Registers" and
       [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception (#PF)". */
    asm ("movl %%cr2, %0" : "=r" (fault_addr));

    /* Turn interrupts back on (they were only off so that we could
       be assured of reading CR2 before it changed). */
    intr_enable();

    /* Count page faults. */
    page_fault_cnt++;

    /* Determine cause. */
    not_present = (f->error_code & PF_P) == 0;
    write = (f->error_code & PF_W) != 0;
    user = (f->error_code & PF_U) != 0;


    /* If the error is because of writing to read only memory then print
     * out the error message and kill the process. */
    if (!not_present) {
        printf("Page fault at %p: %s error %s page in %s context.\n",
           fault_addr,
           not_present ? "not present" : "rights violation",
           write ? "writing" : "reading",
           user ? "user" : "kernel");

        kill(f);
    }


    /* if (!user) */
    /*     printf("Kernel page fault at address %x\n", (unsigned) fault_addr); */
    /* Exit if the page fault was not a user address. */
    /*
    if (!is_user_vaddr(fault_addr)) {
        printf(
            "Page fault at %p: Invalid kernel address %s page in %s context.\n",
            fault_addr,
            write ? "writing" : "reading",
            user ? "user" : "kernel");

        kill(f);
    }
    */

    /* If the error is not present, lookup the page in the supplemental
       page table. */
    struct spte *page_entry = spte_lookup(fault_addr);

    /* printf("Page fault at address %x\n", (unsigned) fault_addr); */
    /* printf("SPTE was found at %x\n", (unsigned) page_entry); */

    /* If the process was not found in the supplemental page entry kill
       the process. Unless it is from growing the stack. */
    if (page_entry == NULL) {

        /* The only way the stackpointer can be above the access will be
           from a push or pusha. */
        if (fault_addr < f->esp && fault_addr != f->esp - 4 &&
            fault_addr != f->esp -32) {
            printf("Page fault at %p: %s error %s page in %s context.\n",
               fault_addr,
               not_present ? "not present" : "rights violation",
               write ? "writing" : "reading",
               user ? "user" : "kernel");

            kill(f);
        }

        /* Only allowing the fault address to be a pgsize away from where
           the current stack pointer is. */
        else if (fault_addr < f->esp - PGSIZE && fault_addr > STACK_FLOOR) {
            printf("Page fault at %p: %s error %s page in %s context.\n",
               fault_addr,
               not_present ? "not present" : "rights violation",
               write ? "writing" : "reading",
               user ? "user" : "kernel");

            kill(f);

        }

        /* printf("%s expanding stack...\n", */
        /*         user ? "User" : "Kernel"); */
        /* If we didn't have a page fault error, then grow the stack. */
        expand_stack(f, fault_addr);

    } else {
        /* Otherwise we found the page in the supplemental page table
         * entry and we just need to get a frame for it. */
        if (!frame_from_spt(page_entry)) {
            printf("Could not load frame.\n");
            kill(f);
        }

    }
}

void * frame_from_spt(struct spte *page_entry)
{
    /* Get the user address for the page, and whether the page was swapped
        before. */
    uint8_t *upage = (uint8_t *) page_entry->uaddr;
    int swap_index = page_entry->swap_index;
    bool writable  = page_entry->writable;

    /* Get a page of memory. */
    struct frame *fr = frame_get_page(upage, PAL_USER);

    /* We pin so that we don't swap out the page while we load its contents */
    frame_pin(fr);

    uint8_t *kpage = fr->kaddr;

    /* printf("Getting frame w/ upage\t%x & kpage %x for thread %s\n", */
    /*         (uint32_t) upage, */
    /*         (uint32_t) kpage, */
    /*         thread_current()->name); */

    if (page_entry->type == PTYPE_MMAP || page_entry->type == PTYPE_CODE) {
        /* printf ("page fault for file code\n"); */
    } else {
        if (page_entry->type == PTYPE_STACK) {
            printf ("page fault for stack %x\n",
                    (unsigned int) upage);
        } else {
            /* printf ("page fault for data \n"); */
        }
    }

    /* We need this lock so that if we were in the process of swapping out
       a page, we know that it is swapped before we check if it is swapped. */
    lock_acquire(&evictlock);

    /* If the page was not swapped, then we page faulted because we still
        need to load the process from file. */
    if (swap_index == NOTSWAPPED) {
        /* printf("Loading from file...\n"); */

        /* Get all of the necessary info to load the process. */
        struct file *file   = page_entry->file;
        off_t ofs           = page_entry->ofs;
        uint32_t read_bytes = page_entry->read_bytes;
        uint32_t zero_bytes = page_entry->zero_bytes;

        file_seek(file, ofs);

        /* Load this page. */
        if (file_read(file, kpage, read_bytes) != (int) read_bytes) {
            printf("Couldn't load the page\n");
            palloc_free_page(kpage);
            return NULL;
        }

        memset(kpage + read_bytes, 0, zero_bytes);

        /* Add the page to the process's address space. */
        if (!install_page(upage, kpage, writable)) {
            printf("Couldn't install the page with upage %x and kpage %x\n",
                    (unsigned int) upage,
                    (unsigned int) kpage);
            palloc_free_page(kpage);
            return NULL;
        }

        page_entry->kaddr = kpage;
        page_entry->loaded = true;
    }

    /* Otherwise, the page was swapped and we need to load it from swap */
    else {

        swap_load(kpage, (block_sector_t) swap_index);

        if (!pagedir_set_page(
                thread_current()->pagedir, upage, kpage, writable)) {
            printf("Couldn't set the page\n");
            palloc_free_page(kpage);
            return NULL;
        }

        page_entry->kaddr = kpage;
        page_entry->swap_index = NOTSWAPPED;
        page_entry->loaded = true;

    }

    frame_unpin(fr);

    lock_release(&evictlock);

    return kpage;
}

/*! When the stack pointer page faults because the stack has run out of room,
    we allocate a new page for the stack, up to some maximum. */
void expand_stack(struct intr_frame *f, void *addr) {
    /* Where the next stack page starts */
    uint8_t *new_stack = (void *) ((unsigned long)
                            addr & (PTMASK | PDMASK));

    /* Getting a page */
    struct frame *fr = frame_get_page(new_stack, PAL_USER | PAL_ZERO);
    frame_pin(fr);
    uint8_t *kpage = fr->kaddr;

    if (kpage == NULL) {
        printf("Couldn't get a frame\n");
        kill(f);
    }

    /* Installing the page */
    if (!install_page(new_stack, kpage, true)) {
        printf("Couldn't install the page\n");
        palloc_free_page(kpage);
        kill(f);
    }

    /* Putting it into the supplemental page table */
    spte_insert(thread_current(), new_stack, kpage, NULL, 0, 0, PGSIZE,
                PTYPE_STACK, true);

    frame_unpin(fr);
}
