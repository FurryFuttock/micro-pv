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
#include <stdint.h>
#include <xen/vcpu.h>

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

static void force_evtchn_callback(void)
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
    // sanity check
    if ((port < 0) || (port >= NUM_CHANNELS))
    {
        PRINTK("ERROR: Invalid port %i\n", port);
        return -1;
    }

    if (ev_actions[port].handler != default_handler)
        PRINTK("WARN: Handler for port %d already registered, replacing\n", port);

    ev_actions[port].data = data;
    wmb();
    ev_actions[port].handler = handler;

    return port;
}

void unbind_evtchn(evtchn_port_t port)
{
    // sanity check
    if ((port < 0) || (port >= NUM_CHANNELS))
    {
        PRINTK("ERROR: Invalid port %i\n", port);
        return;
    }

    ev_actions[port].handler = default_handler;
    wmb();
    ev_actions[port].data = NULL;
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

static void clear_evtchn(uint32_t port)
{
    shared_info_t *s = hypervisor_shared_info;
    synch_clear_bit(port, &s->evtchn_pending[0]);
}

static void mask_evtchn(uint32_t port)
{
    shared_info_t *s = hypervisor_shared_info;
    synch_set_bit(port, &s->evtchn_mask[0]);
}

static void unmask_evtchn(uint32_t port)
{
    shared_info_t *s = hypervisor_shared_info;
    vcpu_info_t *vcpu_info = &s->vcpu_info[smp_processor_id()];

    PRINTK("unmask port %d\n", port);
    synch_clear_bit(port, &s->evtchn_mask[0]);

    /*
     * The following is basically the equivalent of 'hw_resend_irq'. Just like
     * a real IO-APIC we 'lose the interrupt edge' if the channel is masked.
     */
    if (synch_test_bit(port, &s->evtchn_pending[0]) &&
        !synch_test_and_set_bit(port / (sizeof(unsigned long) * 8), &vcpu_info->evtchn_pending_sel))
    {
        vcpu_info->evtchn_upcall_pending = 1;
        if (!vcpu_info->evtchn_upcall_mask)
            force_evtchn_callback();
    }
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

            port = (l1i * (sizeof(unsigned long) * 8)) + l2i;
            do_event(port, regs);
        }
    }
}

void xenevents_init(void)
{
    /* Set all handlers to ignore, and mask them */
    for (unsigned int i = 0; i < NUM_CHANNELS; i++)
    {
        ev_actions[i].handler = default_handler;
        mask_evtchn(i);
    }

    /* Set the event delivery callbacks */
    HYPERVISOR_set_callbacks((unsigned long)hypervisor_callback, (unsigned long)failsafe_callback, 0);
}

evtchn_port_t xenevents_bind_virq(int virq, evtchn_handler_t handler)
{
    evtchn_port_t port = bind_virq(virq, handler, NULL);
    if (port == -1)
    {
        PRINTK("Error initialising VIRQ %i", virq);
        return -1;
    }
    unmask_evtchn(port);
    return port;
}

evtchn_port_t xenevents_bind_channel(int channel, evtchn_handler_t handler)
{
    evtchn_port_t port = bind_evtchn(channel, handler, NULL);
    if (-1 == port)
    {
        PRINTK("Error initialising channel %i", channel);
        return -1;
    }
    unmask_evtchn(port);
    return port;
}

void xenevents_unbind_channel(evtchn_port_t port)
{
    mask_evtchn(port);
    unbind_evtchn(port);
}

void micropv_interrupt_disable(void)
{
    // mask events
    barrier();
    hypervisor_shared_info->vcpu_info[0].evtchn_upcall_mask = 1;
    barrier();
}

void micropv_interrupt_enable(void)
{
    // unmask events
    barrier();
    hypervisor_shared_info->vcpu_info[0].evtchn_upcall_mask = 0;
    barrier();

    // force channel event (if there is one)
    if (hypervisor_shared_info->vcpu_info[0].evtchn_upcall_pending)
        HYPERVISOR_xen_version(0, NULL);
}

int xenevents_alloc_channel(int remote_dom, int *port)
{
    int rc;

    evtchn_alloc_unbound_t op;
    op.dom = DOMID_SELF;
    op.remote_dom = remote_dom;
    rc = HYPERVISOR_event_channel_op(EVTCHNOP_alloc_unbound, &op);
    if (rc)
        PRINTK("ERROR: xenevents_alloc_channel failed with rc=%d", rc);
    else
        *port = op.port;
    return rc;
}

