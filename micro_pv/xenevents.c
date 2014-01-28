/* ********************************************************************
   * Project   :
   * Author    : smartin
   ********************************************************************

    Modifications:
    0.01 06/11/2013 Initial version.
*/

/*---------------------------------------------------------------------
  -- macros (preamble)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes (import)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes (export)
  ---------------------------------------------------------------------*/
#include "xenevents.h"

/*---------------------------------------------------------------------
  -- macros (postamble)
  ---------------------------------------------------------------------*/
#define NUM_CHANNELS (1024)

#define active_evtchns(cpu,sh,idx) ((sh)->evtchn_pending[idx] & ~(sh)->evtchn_mask[idx])

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/
typedef struct _ev_action_t {
    evtchn_handler_t handler;
    void *data;
    uint32_t count;
} ev_action_t;

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/
/**
 * Defined in bootstrap.<arch>.S
 */
void hypervisor_callback(void);
void hypervisor_callback_spm(void);

/**
 * Defined in bootstrap.<arch>.S
 */
void failsafe_callback(void);

void do_hypervisor_callback(struct pt_regs *regs);

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- local variables
  ---------------------------------------------------------------------*/
static ev_action_t ev_actions[NUM_CHANNELS] = { { 0 } };

/*---------------------------------------------------------------------
  -- private functions
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- public functions
  ---------------------------------------------------------------------*/

/*
 * do_exit: This is called whenever an IRET fails in entry.S.
 * This will generally be because an application has got itself into
 * a really bad state (probably a bad CS or SS). It must be killed.
 * Of course, minimal OS doesn't have applications :-)
 */
void do_exit(void)
{
    PRINTK("Do_exit called!\n");
    //stack_walk();
    for (;;)
    {
        struct sched_shutdown sched_shutdown = { .reason = SHUTDOWN_crash };
        HYPERVISOR_sched_op(SCHEDOP_shutdown, &sched_shutdown);
    }
}

void force_evtchn_callback(void)
{
    int save;
    vcpu_info_t *vcpu;
    vcpu = &hypervisor_shared_info->vcpu_info[smp_processor_id()];
    save = vcpu->evtchn_upcall_mask;

    while (vcpu->evtchn_upcall_pending)
    {
        vcpu->evtchn_upcall_mask = 1;
        barrier();
        do_hypervisor_callback(NULL);
        barrier();
        vcpu->evtchn_upcall_mask = save;
        barrier();
    };
}

static void default_handler(evtchn_port_t port, struct pt_regs *regs, void *ignore)
{
    PRINTK("[Port %d] - event received\n", port);
}

evtchn_port_t bind_evtchn(evtchn_port_t port, evtchn_handler_t handler, void *data)
{
    if (ev_actions[port].handler != default_handler)
        PRINTK("WARN: Handler for port %d already registered, replacing\n", port);

    ev_actions[port].data = data;
    wmb();
    ev_actions[port].handler = handler;

    return port;
}

evtchn_port_t bind_virq(uint32_t virq, evtchn_handler_t handler, void *data)
{
    evtchn_bind_virq_t op;
    int rc;

    /* Try to bind the virq to a port */
    op.virq = virq;
    op.vcpu = smp_processor_id();

    if ((rc = HYPERVISOR_event_channel_op(EVTCHNOP_bind_virq, &op)) != 0)
    {
        PRINTK("Failed to bind virtual IRQ %d with rc=%d\n", virq, rc);
        return -1;
    }
    bind_evtchn(op.port, handler, data);
    return op.port;
}

static inline void clear_evtchn(uint32_t port)
{
    shared_info_t *s = hypervisor_shared_info;
    synch_clear_bit(port, &s->evtchn_pending[0]);
}

static int do_event(evtchn_port_t port, struct pt_regs *regs)
{
    ev_action_t  *action;

    clear_evtchn(port);

    if (port >= NUM_CHANNELS)
    {
        PRINTK("WARN: do_event(): Port number too large: %d\n", port);
        return 1;
    }

    action = &ev_actions[port];
    action->count++;

    /* call the handler */
    action->handler(port, regs, action->data);

    return 1;

}

void do_hypervisor_callback(struct pt_regs *regs)
{
    unsigned long  l1, l2, l1i, l2i;
    unsigned int   port;
    int            cpu = 0;
    shared_info_t *s = hypervisor_shared_info;
    vcpu_info_t   *vcpu_info = &s->vcpu_info[cpu];

    // clear the pending mask to catch new events
    vcpu_info->evtchn_upcall_pending = 0;

    /* NB x86. No need for a barrier here -- XCHG is a barrier on x86. */
#if !defined(__i386__) && !defined(__x86_64__)
    /* Clear master flag /before/ clearing selector flag. */
    wmb();
#endif

    /* atomically pull out the pending events and replace it with 0*/
    l1 = xchg(&vcpu_info->evtchn_pending_sel, 0);

    /* process all pending events */
    while (l1 != 0)
    {
        // get the index of the first set bit
        l1i = __ffs(l1);

        // clear this bit
        l1 &= ~(1UL << l1i);

        // get the active
        while ((l2 = active_evtchns(cpu, s, l1i)) != 0)
        {
            l2i = __ffs(l2);
            l2 &= ~(1UL << l2i);

            port = (l1i * (sizeof(unsigned long) * 8)) + l2i;
            do_event(port, regs);
        }
    }
}

void xenevents_init(void)
{
    /* Set the event delivery callbacks */
    HYPERVISOR_set_callbacks((unsigned long)hypervisor_callback, (unsigned long)failsafe_callback, 0);

    /* Set all handlers to ignore, and mask them */
    for (unsigned int i = 0; i < NUM_CHANNELS; i++)
    {
        ev_actions[i].handler = default_handler;
        mask_evtchn(i);
    }

    hypervisor_shared_info->vcpu_info[0].evtchn_upcall_mask = 0;
}

void xenevents_cli(void)
{
    hypervisor_shared_info->vcpu_info[0].evtchn_upcall_mask = 1;
    barrier();
}

void xenevents_sti(void)
{
    barrier();
    hypervisor_shared_info->vcpu_info[0].evtchn_upcall_mask = 0;
    barrier();

    /* unmask then check (avoid races) */
    if (hypervisor_shared_info->vcpu_info[0].evtchn_upcall_pending)
        force_evtchn_callback();
}

