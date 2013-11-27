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
#include <stdint.h>
#include <stdarg.h>
#include <xen/io/console.h>

/*---------------------------------------------------------------------
  -- project includes (export)
  ---------------------------------------------------------------------*/
#include "os.h"
#include "xenconsole.h"
#include "xenevents.h"
#include "psnprintf.h"

/*---------------------------------------------------------------------
  -- macros (postamble)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/
static void xenconsole_event_handler(evtchn_port_t port, struct pt_regs *regs, void *data);

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

static inline struct xencons_interface *xenconsole_interface(void)
{
    return mfn_to_virt(hypervisor_start_info.console.domU.mfn);
}

static inline evtchn_port_t xenconsole_event(void)
{
    return hypervisor_start_info.console.domU.evtchn;
}

static int xenconsole_ring_send_no_notify(struct xencons_interface *ring, const char *data, unsigned len)
{
    int sent = 0;
    int used = 0;

    while ((sent < len) && (used < sizeof(ring->out)))
    {
        // see if we have space for another character
        used = ring->out_prod - ring->out_cons;
        if (used >= sizeof(ring->out))
            break;

        // store another character
        ring->out[ring->out_prod] = data[sent++];
        ring->out_prod = (ring->out_prod + 1) & (sizeof(ring->out) - 1);
    }

    return sent;
}

static int xenconsole_ring_send(struct xencons_interface *ring, evtchn_port_t port, const char *data, unsigned len)
{
    int sent = 0;

    //do
    //{
        sent += xenconsole_ring_send_no_notify(ring, data, len);
        notify_remote_via_evtchn(port);
    //}
    //while (sent < len);

    return sent;
}

int xenconsole_write(const char *format, ...)
{
    va_list args;
    int length;

    // process the format once to get the string length
    va_start(args, format);
    length = pvsnprintf(NULL, 0, format, args);
    va_end(args);

    // if we have a string then print it
    if (length > 0)
    {
        // create the output string
        char message[length + 1];
        va_start(args, format);
        length = pvsnprintf(message, length + 1, format, args);
        va_end(args);

        // send to the console
        xenconsole_ring_send(xenconsole_interface(), xenconsole_event(), message, length);
    }

    return length;
}

static void xenconsole_event_handler(evtchn_port_t port, struct pt_regs *regs, void *data)
{
    struct xencons_interface *ring = xenconsole_interface();

    BUG_ON((ring->in_prod - ring->in_cons) > sizeof(ring->in));
    while (ring->in_cons != ring->in_prod)
    {
        char chr = ring->in[MASK_XENCONS_IDX(ring->in_cons, ring->in)];

        /* Just repeat what's written */
        xenconsole_ring_send(xenconsole_interface(), xenconsole_event(), &chr, 1);

        if (chr == '\r')
            PRINTK("No console input handler.\n");
        ring->in_cons++;
    }
}

/**
 * Initialise the Xen console. The console ring buffer page is already mapped
 * into the guest address space, so we don't need to map the page into our
 * address space.
 *
 * @return 0 if success, -1 otherwise.
 */
int xenconsole_init(void)
{
    if (!xenconsole_event())
        return 0;

    int err = bind_evtchn(xenconsole_event(), xenconsole_event_handler, NULL);
    if (err <= 0)
    {
        PRINTK("XEN console request chn bind failed %i\n", err);
        return -1;
    }
    unmask_evtchn(xenconsole_event());

    /* In case we have in-flight data after save/restore... */
    notify_remote_via_evtchn(xenconsole_event());

    return 0;
}

