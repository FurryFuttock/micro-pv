/* ********************************************************************
   * Project   :
   * Author    : smartin
   ********************************************************************

    Modifications:
    0.01 25/11/2013 Initial version.
*/

/*---------------------------------------------------------------------
  -- macros (preamble)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/
#include <sys/time.h>
#include <stdint.h>
#include <xen/vcpu.h>
#include <string.h>

/*---------------------------------------------------------------------
  -- project includes (import)
  ---------------------------------------------------------------------*/
#include "xenevents.h"
#include "xenschedule.h"

/*---------------------------------------------------------------------
  -- project includes (export)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- macros (postamble)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/
struct shadow_time_info {
    uint64_t tsc_timestamp;     /* TSC at last update of time vals.  */
    uint64_t system_timestamp;  /* Time, in nanosecs, since boot.    */
    uint32_t tsc_to_nsec_mul;
    uint32_t tsc_to_usec_mul;
    int tsc_shift;
    uint32_t version;
};

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- local variables
  ---------------------------------------------------------------------*/
static evtchn_port_t port = -1;
static struct shadow_time_info shadow = { 0 };
static uint32_t shadow_ts_version = 0;
static struct timespec shadow_ts = { 0 };
static uint64_t timer_deadline = 0;
static uint64_t timer_period = TIMER_PERIOD;

/*---------------------------------------------------------------------
  -- private functions
  ---------------------------------------------------------------------*/
static void get_time_values_from_xen(void)
{
    struct vcpu_time_info    *src = &hypervisor_shared_info->vcpu_info[0].time;

    do {
        shadow.version = src->version;
        rmb();
        shadow.tsc_timestamp     = src->tsc_timestamp;
        shadow.system_timestamp  = src->system_time;
        shadow.tsc_to_nsec_mul   = src->tsc_to_system_mul;
        shadow.tsc_shift         = src->tsc_shift;
        rmb();
    }
    while (((volatile uint32_t)src->version & 1) | (shadow.version ^ (volatile uint32_t)src->version));

    shadow.tsc_to_usec_mul = shadow.tsc_to_nsec_mul / 1000;
}

static void update_wallclock(void)
{
    shared_info_t *s = hypervisor_shared_info;

    do {
        shadow_ts_version = s->wc_version;
        rmb();
        shadow_ts.tv_sec  = s->wc_sec;
        shadow_ts.tv_nsec = s->wc_nsec;
        rmb();
    }
    while (((volatile uint32_t)s->wc_version & 1) | (shadow_ts_version ^ (volatile uint32_t)s->wc_version));
}

/*
 * Scale a 64-bit delta by scaling and multiplying by a 32-bit fraction,
 * yielding a 64-bit result.
 */
static inline uint64_t scale_delta(uint64_t delta, uint32_t mul_frac, int shift)
{
    uint64_t product;

    if ( shift < 0 )
        delta >>= -shift;
    else
        delta <<= shift;

    __asm__ (
        "mul %%rdx ; shrd $32,%%rdx,%%rax"
        : "=a" (product) : "0" (delta), "d" ((uint64_t)mul_frac) );

    return product;
}

static unsigned long get_nsec_offset(void)
{
    uint64_t now, delta;
    rdtscll(now);
    delta = now - shadow.tsc_timestamp;
    return scale_delta(delta, shadow.tsc_to_nsec_mul, shadow.tsc_shift);
}

static inline int time_values_up_to_date(void)
{
    struct vcpu_time_info *src = &hypervisor_shared_info->vcpu_info[0].time;

    return (shadow.version == src->version);
}

static int timer_set_next_event()
{
    int cpu = smp_processor_id();
    struct vcpu_set_singleshot_timer single;
    int ret;

    single.timeout_abs_ns = timer_deadline;
    single.flags = VCPU_SSHOTTMR_future;

    ret = HYPERVISOR_vcpu_op(VCPUOP_set_singleshot_timer, cpu, &single);

    return ret;
}

static int timer_stop_periodic()
{
    int cpu = smp_processor_id();
    int ret;

    ret = HYPERVISOR_vcpu_op(VCPUOP_stop_periodic_timer, cpu, NULL);

    return ret;
}

static void timer_handler(evtchn_port_t ev, struct pt_regs *regs, void *ign)
{
    // set the next timer interrupt event. this must be in the fugure
    do timer_deadline += timer_period;
    while (timer_deadline < micropv_time_monotonic_clock());

    // set the next timer event
    timer_set_next_event();

    // housekeeping
    get_time_values_from_xen();
    update_wallclock();

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

/*---------------------------------------------------------------------
  -- public functions
  ---------------------------------------------------------------------*/

/* monotonic_clock(): returns # of nanoseconds passed since time_init()
 *      Note: This function is required to return accurate
 *      time even in the absence of multiple timer ticks.
 */
uint64_t micropv_time_monotonic_clock(void)
{
    uint64_t time;
    uint32_t local_time_version;

    do {
        local_time_version = shadow.version;
        rmb();
        time = shadow.system_timestamp + get_nsec_offset();
        if (!time_values_up_to_date())
            get_time_values_from_xen();
        rmb();
    } while (local_time_version != (volatile uint32_t)shadow.version);

    return time;
}

int micropv_time_gettimeofday(struct timeval *tv, void *tz)
{
    uint64_t nsec = micropv_time_monotonic_clock();
    nsec += shadow_ts.tv_nsec;


    tv->tv_sec = shadow_ts.tv_sec;
    tv->tv_sec += NSEC_TO_SEC(nsec);
    tv->tv_usec = NSEC_TO_USEC(nsec % 1000000000UL);

    return 0;
}

void xentime_init(void)
{
    PRINTK("Initialising timer interface\n");

    // disable the periodic timer event
    timer_stop_periodic();

    // bind the timer virtual IRQ
    port = bind_virq(VIRQ_TIMER, &timer_handler, NULL);
    if (-1 == port)
    {
        PRINTK("Error initialising VIRQ_TIMER");
        return;
    }
    unmask_evtchn(port);

    // initialise the time
    get_time_values_from_xen();
    update_wallclock();

    // initialise the periodic timer
    timer_deadline = micropv_time_monotonic_clock() + timer_period;
    timer_set_next_event();
}

