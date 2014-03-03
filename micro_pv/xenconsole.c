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
    XENCONS_RING_IDX cons, prod;

    cons = ring->out_cons;
    prod = ring->out_prod;
    mb();

    while ((sent < len) && ((prod - cons) < sizeof(ring->out)))
            ring->out[MASK_XENCONS_IDX(prod++, ring->out)] = data[sent++];

    wmb();
    ring->out_prod = prod;

    return sent;
}

static int xenconsole_ring_send(struct xencons_interface *ring, evtchn_port_t port, const char *data, unsigned len)
{
    int sent = 0;

    do
    {
        sent += xenconsole_ring_send_no_notify(ring, data, len);
        notify_remote_via_evtchn(port);
    }
    while (sent < len);

    return sent;
}

int micropv_console_write(const void *ptr, size_t len)
{
    return xenconsole_ring_send(xenconsole_interface(), xenconsole_event(), ptr, len);
}

int micropv_console_write_available()
{
    struct xencons_interface *ring = xenconsole_interface();
    return (ring->out_prod - ring->out_cons - 1) & (sizeof(ring->out) - 1);
}

int xenconsole_printf(const char *format, ...)
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
        micropv_console_write(message, length);
    }

    return length;
}

static void xenconsole_event_handler(evtchn_port_t port, struct pt_regs *regs, void *data)
{
#if 0
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
#endif
}

int micropv_console_read(void *ptr, size_t len)
{
    int received = 0;
    XENCONS_RING_IDX cons, prod;
    struct xencons_interface *ring = xenconsole_interface();

    cons = ring->in_cons;
    prod = ring->in_prod;
    mb();

    while ((received < len) && ((cons - prod) & (sizeof(ring->in) - 1)))
        ((char *)ptr)[received++] = ring->in[MASK_XENCONS_IDX(cons++, ring->in)];

    wmb();
    ring->in_cons = cons;

    return received;
}

int micropv_console_read_available()
{
    struct xencons_interface *ring = xenconsole_interface();
    return (ring->in_cons - ring->in_prod) & (sizeof(ring->in) - 1);
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

