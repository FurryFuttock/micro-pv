/* ********************************************************************
   * Project   :
   * Author    : smartin
   ********************************************************************

    Modifications:
    0.01 01/11/2013 Initial version.
*/

/*---------------------------------------------------------------------
  -- macros (preamble)
  ---------------------------------------------------------------------*/
#define read_cr2() (hypervisor_shared_info->vcpu_info[smp_processor_id()].arch.cr2)

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/
#include <stdint.h>
#include <xen/xen.h>

/*---------------------------------------------------------------------
  -- project includes (import)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes (export)
  ---------------------------------------------------------------------*/
#include "hypercall.h"
#include "hypervisor.h"
#include "traps.h"
#include "../micropv.h"

/*---------------------------------------------------------------------
  -- macros (postamble)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/
/*
 * These are assembler stubs in entry.S.
 * They are the actual entry points for virtual exceptions.
 */
void divide_error(void);
void debug(void);
void int3(void);
void overflow(void);
void bounds(void);
void invalid_op(void);
void device_not_available(void);
void coprocessor_segment_overrun(void);
void invalid_TSS(void);
void segment_not_present(void);
void stack_segment(void);
void general_protection(void);
void page_fault(void);
void coprocessor_error(void);
void simd_coprocessor_error(void);
void alignment_check(void);
void spurious_interrupt_bug(void);
void machine_check(void);

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/
uint64_t(*micropv_traps_fp_context)(struct pt_regs *regs) = NULL;

/*---------------------------------------------------------------------
  -- local variables
  ---------------------------------------------------------------------*/
static trap_info_t trap_table[] = {
    {  0, 0, FLAT_KERNEL_CS, (unsigned long)divide_error                },
    {  1, 0, FLAT_KERNEL_CS, (unsigned long)debug                       },
    {  3, 3, FLAT_KERNEL_CS, (unsigned long)int3                        },
    {  4, 3, FLAT_KERNEL_CS, (unsigned long)overflow                    },
    {  5, 3, FLAT_KERNEL_CS, (unsigned long)bounds                      },
    {  6, 0, FLAT_KERNEL_CS, (unsigned long)invalid_op                  },
    {  7, 0, FLAT_KERNEL_CS, (unsigned long)device_not_available        },
    {  9, 0, FLAT_KERNEL_CS, (unsigned long)coprocessor_segment_overrun },
    { 10, 0, FLAT_KERNEL_CS, (unsigned long)invalid_TSS                 },
    { 11, 0, FLAT_KERNEL_CS, (unsigned long)segment_not_present         },
    { 12, 0, FLAT_KERNEL_CS, (unsigned long)stack_segment               },
    { 13, 0, FLAT_KERNEL_CS, (unsigned long)general_protection          },
    { 14, 0, FLAT_KERNEL_CS, (unsigned long)page_fault                  },
    { 15, 0, FLAT_KERNEL_CS, (unsigned long)spurious_interrupt_bug      },
    { 16, 0, FLAT_KERNEL_CS, (unsigned long)coprocessor_error           },
    { 17, 0, FLAT_KERNEL_CS, (unsigned long)alignment_check             },
    { 19, 0, FLAT_KERNEL_CS, (unsigned long)simd_coprocessor_error      },
    {  0, 0,           0, 0                           }
};

/*---------------------------------------------------------------------
  -- private functions
  ---------------------------------------------------------------------*/
static void dump_regs(struct pt_regs *regs)
{
    PRINTK("-------------------- REGISTER FILE --------------------");
    PRINTK("RIP: %04lx:%016lx ", regs->cs & 0xffff, regs->rip);
    PRINTK("RSP: %04lx:%016lx  EFLAGS: %08lx",
           regs->ss, regs->rsp, regs->eflags);
    PRINTK("RAX: %016lx RBX: %016lx RCX: %016lx",
           regs->rax, regs->rbx, regs->rcx);
    PRINTK("RDX: %016lx RSI: %016lx RDI: %016lx",
           regs->rdx, regs->rsi, regs->rdi);
    PRINTK("RBP: %016lx R08: %016lx R09: %016lx",
           regs->rbp, regs->r8, regs->r9);
    PRINTK("R10: %016lx R11: %016lx R12: %016lx",
           regs->r10, regs->r11, regs->r12);
    PRINTK("R13: %016lx R14: %016lx R15: %016lx",
           regs->r13, regs->r14, regs->r15);
}

static void dump_fp_regs(struct pt_regs *regs)
{
    uint64_t xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
    uint64_t xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15, mxcsr;
    __asm__(" movq %%xmm0,%0" : "=m" (xmm0));
    __asm__(" movq %%xmm1,%0" : "=m" (xmm1));
    __asm__(" movq %%xmm2,%0" : "=m" (xmm2));
    __asm__(" movq %%xmm3,%0" : "=m" (xmm3));
    __asm__(" movq %%xmm4,%0" : "=m" (xmm4));
    __asm__(" movq %%xmm5,%0" : "=m" (xmm5));
    __asm__(" movq %%xmm6,%0" : "=m" (xmm6));
    __asm__(" movq %%xmm7,%0" : "=m" (xmm7));
    __asm__(" movq %%xmm8,%0" : "=m" (xmm8));
    __asm__(" movq %%xmm9,%0" : "=m" (xmm9));
    __asm__(" movq %%xmm10,%0" : "=m" (xmm10));
    __asm__(" movq %%xmm11,%0" : "=m" (xmm11));
    __asm__(" movq %%xmm12,%0" : "=m" (xmm12));
    __asm__(" movq %%xmm13,%0" : "=m" (xmm13));
    __asm__(" movq %%xmm14,%0" : "=m" (xmm14));
    __asm__(" movq %%xmm15,%0" : "=m" (xmm15));
    __asm__(" stmxcsr %0" : "=m" (mxcsr));
    PRINTK("XMM0:  %016lx XMM1:  %016lx XMM2:  %016lx", xmm0, xmm1, xmm2);
    PRINTK("XMM3:  %016lx XMM4:  %016lx XMM5:  %016lx", xmm3, xmm4, xmm5);
    PRINTK("XMM6:  %016lx XMM7:  %016lx XMM8:  %016lx", xmm6, xmm7, xmm8);
    PRINTK("XMM9:  %016lx XMM10: %016lx XMM11: %016lx", xmm9, xmm10, xmm11);
    PRINTK("XMM12: %016lx XMM13: %016lx XMM14: %016lx", xmm12, xmm13, xmm14);
    PRINTK("XMM15: %016lx MXCSR: %016lx", xmm15, mxcsr);
}

static void do_stack_walk(unsigned long frame_base)
{
    PRINTK("-------------------- STACK WALK    --------------------");
    unsigned long *frame = (void*) frame_base;
    PRINTK("base is %#lx ", frame_base);
    PRINTK("caller is %#lx", frame[1]);
    //if (frame[0])
    //    do_stack_walk(frame[0]);
}

static void dump_mem(unsigned long addr)
{
    PRINTK("-------------------- MEMORY        --------------------");
    unsigned long i;
    if (addr < __PAGE_SIZE)
        return;

    for (i = ((addr)-16 ) & ~15; i < (((addr)+48 ) & ~15); )
    {
        static char hexchar[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
        char buffer[68];
        int j;

        // store address
        for (j = 0; j < 16; j++)
            buffer[15 - j] = hexchar[(i >> (j * 4)) & 0xf];
        buffer[16] = ':';
        buffer[17] = ' ';

        // 16 bytes of data
        for (j = 0; (j < 48); j += 3, i++)
        {
            buffer[18 + j    ] = hexchar[((*(unsigned char*)i) >> 4) & 0xf];
            buffer[18 + j + 1] = hexchar[((*(unsigned char*)i) >> 0) & 0xf];
            buffer[18 + j + 2] = ' ';
        }
        buffer[67] = 0;
        PRINTK("%s", buffer);
    }
}

static void dump_context(struct pt_regs *regs)
{
    // log context
    dump_regs(regs);
    dump_fp_regs(regs);
    do_stack_walk(regs->rbp);
    dump_mem(regs->rsp);
    dump_mem(regs->rbp);
    dump_mem(regs->rip);

    // stop
    struct sched_shutdown sched_shutdown = { .reason = SHUTDOWN_crash };
    HYPERVISOR_sched_op(SCHEDOP_shutdown, &sched_shutdown);
}

/*---------------------------------------------------------------------
  -- public functions
  ---------------------------------------------------------------------*/

/* Dummy implementation.  Should actually do something */
void do_divide_error(struct pt_regs *regs)                  { PRINTK("%s", __FUNCTION__); dump_context(regs); }
void do_debug(struct pt_regs *regs)                         { PRINTK("%s", __FUNCTION__); dump_context(regs); }
void do_int3(struct pt_regs *regs)                          { PRINTK("%s", __FUNCTION__); dump_context(regs); }
void do_overflow(struct pt_regs *regs)                      { PRINTK("%s", __FUNCTION__); dump_context(regs); }
void do_bounds(struct pt_regs *regs)                        { PRINTK("%s", __FUNCTION__); dump_context(regs); }
void do_invalid_op(struct pt_regs *regs)                    { PRINTK("%s", __FUNCTION__); dump_context(regs); }

void do_device_not_available(struct pt_regs *regs)
{
    // if we have a function to handle this then call it
    if (micropv_traps_fp_context)
        micropv_traps_fp_context(regs);
    // we don't have a handler so crash
    else
    {
        PRINTK("%s", __FUNCTION__);
        dump_context(regs);
    }
}

void do_coprocessor_segment_overrun(struct pt_regs *regs)   { PRINTK("%s", __FUNCTION__); dump_context(regs); }
void do_invalid_TSS(struct pt_regs *regs)                   { PRINTK("%s", __FUNCTION__); dump_context(regs); }
void do_segment_not_present(struct pt_regs *regs)           { PRINTK("%s", __FUNCTION__); dump_context(regs); }
void do_stack_segment(struct pt_regs *regs)                 { PRINTK("%s", __FUNCTION__); dump_context(regs); }
void do_general_protection(struct pt_regs *regs)            { PRINTK("%s", __FUNCTION__); dump_context(regs); }

void do_page_fault(struct pt_regs *regs, unsigned long error_code)
{
    PRINTK("%s", __FUNCTION__);

    unsigned long addr = read_cr2();
    static volatile int handling_pg_fault = 0;

    /* If we are already handling a page fault, and got another one
       that means we faulted in pagetable walk. Continuing here would cause
       a recursive fault */
    if(handling_pg_fault == 1)
    {
        PRINTK("Page fault in pagetable walk (access to invalid memory?).");
        struct sched_shutdown sched_shutdown = { .reason = SHUTDOWN_crash };
        HYPERVISOR_sched_op(SCHEDOP_shutdown, &sched_shutdown);
    }
    handling_pg_fault++;
    barrier();

    PRINTK("Page fault at linear address %p, rip %p, regs %p, sp %p, our_sp %p, code %lx",
           (void *)addr, (void *)regs->rip, regs, (void *)regs->rsp, &addr, error_code);

    dump_context(regs);

    /* We should never get here ... but still */
    handling_pg_fault--;
}

void do_coprocessor_error(struct pt_regs *regs)             { PRINTK("%s", __FUNCTION__); dump_context(regs); }
void do_simd_coprocessor_error(struct pt_regs *regs)
{
    PRINTK("%s", __FUNCTION__);
    dump_context(regs);
    //__asm__ volatile (" fninit");
}
void do_alignment_check(struct pt_regs *regs)               { PRINTK("%s", __FUNCTION__); dump_context(regs); }
void do_spurious_interrupt_bug(struct pt_regs *regs)        { PRINTK("%s", __FUNCTION__); dump_context(regs); }
void do_machine_check(struct pt_regs *regs)                 { PRINTK("%s", __FUNCTION__); dump_context(regs); }

/*
 * Submit a virtual IDT to teh hypervisor. This consists of tuples
 * (interrupt vector, privilege ring, CS:EIP of handler).
 * The 'privilege ring' field specifies the least-privileged ring that
 * can trap to that vector using a software-interrupt instruction (INT).
 */

void xentraps_init(void)
{
    HYPERVISOR_set_trap_table(trap_table);
}

