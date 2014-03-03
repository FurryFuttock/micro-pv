/* ***********************************************************************
   * Project:
   * File: xenschedule.c
   * Author: smartin
   ***********************************************************************

    Modifications
    0.00 3/3/2014 created
*/

/*---------------------------------------------------------------------
  -- macros (preamble)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/
#include <string.h>
#include <string.h>
#include <stdint.h>

/*---------------------------------------------------------------------
  -- project includes (imports)
  ---------------------------------------------------------------------*/
#include "hypercall.h"
#include "../micro_pv.h"

/*---------------------------------------------------------------------
  -- project includes (exports)
  ---------------------------------------------------------------------*/
#include "xenschedule.h"

/*---------------------------------------------------------------------
  -- macros (postamble)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- forward declarations
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/
static uint64_t scheduler_callback_dummy(struct pt_regs *regs);

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/
uint64_t (*micropv_scheduler_callback)(struct pt_regs *regs) = scheduler_callback_dummy;

/*---------------------------------------------------------------------
  -- local variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- implementation
  ---------------------------------------------------------------------*/

static uint64_t scheduler_callback_dummy(struct pt_regs *regs)
{
    return TIMER_PERIOD;
}

void micropv_scheduler_initialise_context(struct pt_regs *regs, void *start_ptr, void *stack_ptr, int stack_size)
{
    // zero the register file
    memset(regs, 0,  sizeof(struct pt_regs));

    // we assume that everything is in the same data area, so we initialise the stack segment and
    // code segment to the same as we have
    { uint64_t ss; __asm__("\t movq %%ss,%0" : "=r"(ss)); regs->ss = ss; }  // preserve the current stack segment
    regs->rsp = (uint64_t)(stack_ptr + stack_size);                         // top of stack
    regs->eflags = 0x200;                                               // flags - enable interrupts
    { uint64_t cs; __asm__("\t movq %%cs,%0" : "=r"(cs)); regs->cs = cs; }  // preserve the current code segment
    regs->rip = (uint64_t)start_ptr;                                    // instruction pointer
}

void micropv_scheduler_yield()
{
    HYPERVISOR_sched_op(SCHEDOP_yield, 0);
}

void micropv_scheduler_block()
{
    HYPERVISOR_sched_op(SCHEDOP_block, 0);
}

