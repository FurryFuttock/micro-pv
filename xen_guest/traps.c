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

/*---------------------------------------------------------------------
  -- public functions
  ---------------------------------------------------------------------*/

/* Dummy implementation.  Should actually do something */
void do_divide_error(struct pt_regs *regs)                  { PRINTK("%s\n", __FUNCTION__); }
void do_debug(struct pt_regs *regs)                         { PRINTK("%s\n", __FUNCTION__); }
void do_int3(struct pt_regs *regs)                          { PRINTK("%s\n", __FUNCTION__); }
void do_overflow(struct pt_regs *regs)                      { PRINTK("%s\n", __FUNCTION__); }
void do_bounds(struct pt_regs *regs)                        { PRINTK("%s\n", __FUNCTION__); }
void do_invalid_op(struct pt_regs *regs)                    { PRINTK("%s\n", __FUNCTION__); }
void do_device_not_available(struct pt_regs *regs)          { PRINTK("%s\n", __FUNCTION__); }
void do_coprocessor_segment_overrun(struct pt_regs *regs)   { PRINTK("%s\n", __FUNCTION__); }
void do_invalid_TSS(struct pt_regs *regs)                   { PRINTK("%s\n", __FUNCTION__); }
void do_segment_not_present(struct pt_regs *regs)           { PRINTK("%s\n", __FUNCTION__); }
void do_stack_segment(struct pt_regs *regs)                 { PRINTK("%s\n", __FUNCTION__); }
void do_general_protection(struct pt_regs *regs)            { PRINTK("%s\n", __FUNCTION__); }
void do_page_fault(struct pt_regs *regs)                    { PRINTK("%s\n", __FUNCTION__); }
void do_coprocessor_error(struct pt_regs *regs)             { PRINTK("%s\n", __FUNCTION__); }
void do_simd_coprocessor_error(struct pt_regs *regs)        { PRINTK("%s\n", __FUNCTION__); }
void do_alignment_check(struct pt_regs *regs)               { PRINTK("%s\n", __FUNCTION__); }
void do_spurious_interrupt_bug(struct pt_regs *regs)        { PRINTK("%s\n", __FUNCTION__); }
void do_machine_check(struct pt_regs *regs)                 { PRINTK("%s\n", __FUNCTION__); }

/*
 * Submit a virtual IDT to teh hypervisor. This consists of tuples
 * (interrupt vector, privilege ring, CS:EIP of handler).
 * The 'privilege ring' field specifies the least-privileged ring that
 * can trap to that vector using a software-interrupt instruction (INT).
 */

void hypervisor_trap_init(void)
{
    HYPERVISOR_set_trap_table(trap_table);
}

