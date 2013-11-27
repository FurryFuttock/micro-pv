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
#define NSEC_TO_SEC(_nsec)      ((_nsec) / 1000000000ULL)
#define NSEC_TO_USEC(_nsec)     ((_nsec) / 1000UL)

#define USEC_TO_NSEC(_usec)     ((_usec) * 1000UL)
#define MSEC_TO_NSEC(_msec)     USEC_TO_NSEC((_msec) * 1000UL)

#define TIMER_PERIOD            USEC_TO_NSEC(1000L)

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/
#include <sys/time.h>
#include <stdint.h>
#include <xen/vcpu.h>

/*---------------------------------------------------------------------
  -- project includes (import)
  ---------------------------------------------------------------------*/
#include "xenevents.h"

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

static uint64_t last_timer_deadline = 0;
static uint64_t last_timer = 0;

int64_t virq_latency_min_nsec = 0xffffffff;
int64_t virq_latency_max_nsec = 0;
int64_t virq_min_nsec = 0xffffffff;
int64_t virq_max_nsec = 0;

/*---------------------------------------------------------------------
  -- private functions
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- public functions
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

/* monotonic_clock(): returns # of nanoseconds passed since time_init()
 *      Note: This function is required to return accurate
 *      time even in the absence of multiple timer ticks.
 */
uint64_t monotonic_clock(void)
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

int xentime_gettimeofday(struct timeval *tv, void *tz)
{
    uint64_t nsec = monotonic_clock();
    nsec += shadow_ts.tv_nsec;


    tv->tv_sec = shadow_ts.tv_sec;
    tv->tv_sec += NSEC_TO_SEC(nsec);
    tv->tv_usec = NSEC_TO_USEC(nsec % 1000000000UL);

    return 0;
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

static void timer_handler(evtchn_port_t ev, struct pt_regs *regs, void *ign)
{
    // get now
    uint64_t now = monotonic_clock();

    // update the tick count
    extern uint64_t ticks_count;
    ticks_count++;

    // housekeeping
    get_time_values_from_xen();
    update_wallclock();

    // statistics
    int64_t latency = now - timer_deadline;
    if (latency > 0)
    {
        // latency
        if (virq_latency_max_nsec < latency)
            virq_latency_max_nsec = latency;
        if (virq_latency_min_nsec > latency)
            virq_latency_min_nsec = latency;
        last_timer_deadline = timer_deadline;

        // period
        int64_t period = now - last_timer;
        if (virq_max_nsec < period)
            virq_max_nsec = period;
        if (virq_min_nsec > period)
            virq_min_nsec = period;
        last_timer = now;
    }

    // set the next timer
    timer_deadline += TIMER_PERIOD;
    timer_set_next_event();
}

void xentime_init(void)
{
    PRINTK("Initialising timer interface\n");

    // initialise the time
    get_time_values_from_xen();
    update_wallclock();

    // bind the timer virtual IRQ
    port = bind_virq(VIRQ_TIMER, &timer_handler, NULL);
    if (-1 == port)
    {
        PRINTK("Error initialising VIRQ_TIMER");
        return;
    }
    unmask_evtchn(port);

    // initialise the periodic timer
    timer_deadline = (last_timer = last_timer_deadline = monotonic_clock()) + TIMER_PERIOD;
    timer_set_next_event();
}

