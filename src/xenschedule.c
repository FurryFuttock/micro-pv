/*  ***********************************************************************
    * Project:
    * File: xenschedule.c
    * Author: smartin
    ***********************************************************************

    This works in tandem with xenevents to produce a preemptive scheduler.

    I was told by the Xen maillist not to use the periodic timer as this is a really low priority, but to use the singleshot
    timer instead.

    To avoid the singleshot timer deadline wandering over time, it is initialised at startup and we then just increment from
    there.

    This doesn't actually do any context switching as we don't know about contexts here, that must be handled by the overlying
    OS, however we do set the HYPERVISOR_fpu_taskswitch so that the overlying OS can implement lazy FP/SSE context handling

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
#include <stdint.h>

/*---------------------------------------------------------------------
  -- project includes (imports)
  ---------------------------------------------------------------------*/
#include "hypercall.h"
#include "xentime.h"
#include "xenevents.h"
#include "../micropv.h"

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
static uint64_t timer_deadline = 0;
static uint64_t timer_period = TIMER_PERIOD;
static evtchn_port_t port = -1;

/*---------------------------------------------------------------------
  -- implementation
  ---------------------------------------------------------------------*/

static uint64_t scheduler_callback_dummy(struct pt_regs *regs)
{
    return TIMER_PERIOD;
}

static void timer_handler(evtchn_port_t ev, struct pt_regs *regs, void *ign)
{
    // set the next timer interrupt event. this must be in the fugure
    do timer_deadline += timer_period;
    while (timer_deadline < micropv_time_monotonic_clock());

    // set the next timer event
    xentime_set_next_event(timer_deadline);

    // housekeeping
    xentime_update();

    // store the current stack data
    unsigned long rsp = regs->rsp;
    unsigned long ss = regs->ss;

    // call the guest OS handler. The guest returns the time to the next interrupt,
    // and will alter the register file if it want's to perform a context switch.
    timer_period = micropv_scheduler_callback(regs);

    // if the timer irq changed the stack then apply the changes
    if ((rsp != regs->rsp) || (ss != regs->ss))
    {
        // switch stacks in the hypervisor
        HYPERVISOR_stack_switch(regs->ss, regs->rsp);

        // set CR0.TS so that the next time that there is an SSE (floating point) access
        // we get a device_disabled trap
        HYPERVISOR_fpu_taskswitch(1);
    }
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

void xenscheduler_init(void)
{
    // disable the periodic timer event
    xentime_stop_periodic();

    // bind the timer virtual IRQ
    port = xenevents_bind_virq(VIRQ_TIMER, &timer_handler);

    // initialise the periodic timer
    timer_deadline = micropv_time_monotonic_clock() + timer_period;
    xentime_set_next_event(timer_deadline);
}

