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
#define BUG do_exit

#define ASSERT(x)                                              \
do {                                                           \
    if (!(x)) {                                                \
        PRINTK("ASSERTION FAILED: %s at %s:%d.\n",             \
               # x ,                                           \
               __FILE__,                                       \
               __LINE__);                                      \
        BUG();                                                 \
    }                                                          \
} while(0)

#define BUG_ON(x) ASSERT(!(x))

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/
#include <stdint.h>
#include <xen/event_channel.h>

/*---------------------------------------------------------------------
  -- project includes (import)
  ---------------------------------------------------------------------*/
#include "hypervisor.h"
#include "hypercall.h"
#include "traps.h"

/*---------------------------------------------------------------------
  -- project includes (export)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- macros (postamble)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/
typedef void (*evtchn_handler_t)(evtchn_port_t, struct pt_regs*, void*);

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/
void xenevents_init(void);
evtchn_port_t bind_evtchn(evtchn_port_t port, evtchn_handler_t handler, void *data);
evtchn_port_t bind_virq(uint32_t virq, evtchn_handler_t handler, void *data);
void do_exit(void);
void force_evtchn_callback(void);

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- local variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- private functions
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- public functions
  ---------------------------------------------------------------------*/

static inline int notify_remote_via_evtchn(evtchn_port_t port)
{
    evtchn_send_t op;
    op.port = port;
    return HYPERVISOR_event_channel_op(EVTCHNOP_send, &op);
}

static inline void mask_evtchn(uint32_t port)
{
    shared_info_t *s = hypervisor_shared_info;
    synch_set_bit(port, &s->evtchn_mask[0]);
}

static inline void unmask_evtchn(uint32_t port)
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

