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
#define BUG micropv_exit

#define ASSERT(x)                                              \
do {                                                           \
    if (!(x)) {                                                \
        PRINTK("ASSERTION FAILED: %s at %s:%d.",             \
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
#include "../micropv.h"

/*---------------------------------------------------------------------
  -- project includes (export)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- macros (postamble)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/
/**
 * Initialise the Xen event interface.
 */
void xenevents_init(void);

/**
 * Bind a function to VIRQ event.
 *
 * @param virq    Xen VIRQ
 * @param handler Function to be bound
 *
 * @return -1 if falure, otherwise the associted port number
 */
evtchn_port_t xenevents_bind_virq(int virq, evtchn_handler_t probe);

/**
 * Bind a function to an event channel.
 *
 * @param channel Xen event channel
 * @param handler Function to be bound
 *
 * @return -1 if falure, otherwise the associted port number
 */
evtchn_port_t xenevents_bind_handler(int channel, evtchn_handler_t handler);

/**
 * Ask the hypervisor to give us a new channel
 *
 * @author smartin (7/9/2014)
 *
 * @param remote_dom dom we want to talk to
 * @param channel assigned channel
 *
 * @return int 0 if success, otherwise -1
 */
int xenevents_alloc_channel(int remote_dom, int *channel);

/**
 * Release an event channel
 *
 * @author smartin (7/9/2014)
 *
 * @param port port to free
 */
void xenevents_unbind_channel(evtchn_port_t port);

/**
 * Create an event channel and assign the callback
 *
 * @param event_channel
 *                   Id of the created event channel.
 * @param event_port This mus be used to signal the event.
 * @param event_handler
 *                   Callback function
 *
 * @return 0 => success, otherwise fail
 */
int xenevents_create_event(evtchn_port_t *event_port, evtchn_handler_t event_handler);

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
/**
 * notify the remote domain.
 */
static inline int xenevents_notify_remote_via_evtchn(evtchn_port_t port)
{
    evtchn_send_t op;
    op.port = port;
    return HYPERVISOR_event_channel_op(EVTCHNOP_send, &op);
}

