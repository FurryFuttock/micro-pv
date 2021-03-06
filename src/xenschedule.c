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
static uint64_t scheduler_timer_dummy(struct pt_regs *regs, uint64_t deadliner);
static void scheduler_yield_dummy(struct pt_regs *regs);

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/
uint64_t (*micropv_scheduler_timer_callback)(struct pt_regs *regs, uint64_t deadline) = scheduler_timer_dummy;
void (*micropv_scheduler_yield_callback)(struct pt_regs *regs) = scheduler_yield_dummy;

/*---------------------------------------------------------------------
  -- local variables
  ---------------------------------------------------------------------*/
static uint64_t timer_deadline = 0;
static uint64_t timer_period = TIMER_PERIOD;
static evtchn_port_t timer_port = -1;
static evtchn_port_t yield_port = -1;
static int in_yield = 0;

/*---------------------------------------------------------------------
  -- implementation
  ---------------------------------------------------------------------*/

static uint64_t scheduler_timer_dummy(struct pt_regs *regs, uint64_t deadline)
{
    return TIMER_PERIOD;
}

static void scheduler_yield_dummy(struct pt_regs *regs)
{
}

static void yield_handler(uint32_t port, struct pt_regs *regs, void *context)
{
    // only run this if we are the result of a yield. When you bind to an event channel you get an initial call to the handler
    if (in_yield)
    {
        // clear the yield flag
        in_yield = 0;

        // store the current stack data
        unsigned long sp = regs->sp;
        unsigned long ss = regs->ss;

        // call handler
        micropv_scheduler_yield_callback(regs);

        // if the timer irq changed the stack then apply the changes
        if ((sp != regs->sp) || (ss != regs->ss))
        {
            // switch stacks in the hypervisor
            HYPERVISOR_stack_switch(regs->ss, regs->sp);

            // set CR0.TS so that the next time that there is an SSE (floating point) access
            // we get a device_disabled trap
            HYPERVISOR_fpu_taskswitch(1);
        }
    }
}

static void timer_handler(evtchn_port_t ev, struct pt_regs *regs, void *ign)
{
    // store the current deadline so we can pass it down
    uint64_t deadline = timer_deadline;

    // set the next timer interrupt event. this must be in the future
    do timer_deadline += timer_period;
    while (timer_deadline < micropv_time_monotonic_clock());

    // set the next timer event
    xentime_set_next_event(timer_deadline);

    // housekeeping
    xentime_update();

    // store the current stack data
    unsigned long sp = regs->sp;
    unsigned long ss = regs->ss;

    // call the guest OS handler. The guest returns the time to the next interrupt,
    // and will alter the register file if it want's to perform a context switch.
    timer_period = (micropv_scheduler_timer_callback ? micropv_scheduler_timer_callback : scheduler_timer_dummy)(regs, deadline);

    // if the timer irq changed the stack then apply the changes
    if ((sp != regs->sp) || (ss != regs->ss))
    {
        // switch stacks in the hypervisor
        HYPERVISOR_stack_switch(regs->ss, regs->sp);

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
    regs->sp = (uint64_t)(stack_ptr + stack_size) & ~0xf;                  // top of stack (16 byte aligned)
    regs->flags = 0x200;                                               // flags - enable interrupts
    { uint64_t cs; __asm__("\t movq %%cs,%0" : "=r"(cs)); regs->cs = cs; }  // preserve the current code segment
    regs->ip = (uint64_t)start_ptr;                                    // instruction pointer
}

void micropv_scheduler_yield(void)
{
    if (-1 != yield_port)
    {
        in_yield = 1;
        if (micropv_fire_event(yield_port))
            PRINTK("Error firing context_switch_event");
    }
}

void micropv_scheduler_block(void)
{
    HYPERVISOR_sched_op(SCHEDOP_block, 0);
}

void xenscheduler_init(void)
{
    // disable the periodic timer event
    xentime_stop_periodic();

    // bind the timer virtual IRQ
    timer_port = xenevents_bind_virq(VIRQ_TIMER, &timer_handler);

    // create the yield event
    xenevents_create_event(&yield_port, yield_handler);

    // initialise the periodic timer
    timer_deadline = micropv_time_monotonic_clock() + timer_period;
    xentime_set_next_event(timer_deadline);
}

void micropv_exit(void)
{
    PRINTK("micropv_exit called!");

    for (;;)
    {
        struct sched_shutdown sched_shutdown = { .reason = SHUTDOWN_reboot };
        HYPERVISOR_sched_op(SCHEDOP_shutdown, &sched_shutdown);
    }
}

