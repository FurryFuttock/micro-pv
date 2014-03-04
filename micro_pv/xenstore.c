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
#define MIN(x, y) ((x) < (y) ? (x) : (y))

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/
#include <stdint.h>
#include <string.h>
#include <xen/io/xs_wire.h>

/*---------------------------------------------------------------------
  -- project includes (import)
  ---------------------------------------------------------------------*/
#include "xenevents.h"
#include "xenconsole.h"
#include "coroutine.h"
#include "psnprintf.h"

/*---------------------------------------------------------------------
  -- project includes (export)
  ---------------------------------------------------------------------*/
#include "xenstore.h"

/*---------------------------------------------------------------------
  -- macros (postamble)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- local variables
  ---------------------------------------------------------------------*/
static int xenstore_req_id = 0;
static char xenstore_dump[XENSTORE_RING_SIZE] = { 0 };
static char hypervisor_domid[6];
static evtchn_port_t port = -1;

/*---------------------------------------------------------------------
  -- private functions
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- public functions
  ---------------------------------------------------------------------*/

static inline struct xenstore_domain_interface *xenstore_interface(void)
{
    return mfn_to_virt(hypervisor_start_info.store_mfn);
}

static inline evtchn_port_t xenstore_event(void)
{
    return hypervisor_start_info.store_evtchn;
}

/* Write a request to the back end */
static int xenstore_write_request(const char *message, int length)
{
    /* Check that the message will fit */
    if(length > XENSTORE_RING_SIZE)
        return -1;

    struct xenstore_domain_interface *xenstore = xenstore_interface();
    int i;
    for(i=xenstore->req_prod ; length > 0 ; i++,length--)
    {
        /* Wait for the back end to clear enough space in the buffer */
        XENSTORE_RING_IDX data;
        do
        {
            data = i - xenstore->req_cons;
            mb();
        } while (data >= sizeof(xenstore->req));
        /* Copy the byte */
        int ring_index = MASK_XENSTORE_IDX(i);
        xenstore->req[ring_index] = *message;
        message++;
    }

    /* Ensure that the data really is in the ring before continuing */
    wmb();

    // update the index
    xenstore->req_prod = i;

    // return success
    return 0;
}

/* Read a response from the response ring */
static int xenstore_read_response(char * message, int length)
{
    struct xenstore_domain_interface *xenstore = xenstore_interface();
    int i;
    for(i=xenstore->rsp_cons ; length > 0 ; i++,length--)
    {
        /* Wait for the back end put data in the buffer */
        XENSTORE_RING_IDX data;
        do
        {
            data = xenstore->rsp_prod - i;
            mb();
        } while (data == 0);

        /* Copy the byte */
        int ring_index = MASK_XENSTORE_IDX(i);
        *message = xenstore->rsp[ring_index];
        message++;
    }

    // update the index
    xenstore->rsp_cons = i;

    return 0;
}

int xenstore_transact(coroutine_context_t *coroutine_context, const void *request, size_t request_length, char *response, size_t response_size, size_t *response_length)
{
    // write the message
    COROUTINE_BEGIN;
    while (request && request_length)
        COROUTINE_RETURN(xenstore_write_request(request, request_length));
    COROUTINE_END;

    // notify the back end
    xenevents_notify_remote_via_evtchn(xenstore_event());

    // read the response header
    struct xsd_sockmsg msg = { 0 };
    xenstore_read_response((char*)&msg, sizeof(msg));

    // read any response data
    if (msg.len > 0)
    {
        // read the requested response
        if (response && response_size && (msg.type != XS_ERROR))
        {
            register size_t length = MIN(msg.len, response_size);
            if (response_length)
                *response_length = length;
            xenstore_read_response(response, length);
            PRINTK("%s response %i %.*s\n", coroutine_context->caller, (int)length, (int)length, response);
        }

        // read the remainder
        if (response_size < msg.len)
        {
            xenstore_read_response(xenstore_dump, msg.len - response_size);
            xenstore_dump[msg.len - response_size] = 0;
        }
    }

    // we are out of sync so fail
    if (msg.req_id != xenstore_req_id)
    {
        PRINTK("%s invalid request id\n", coroutine_context->caller);
        return -1;
    }

    // report errors
    if (msg.type == XS_ERROR)
    {
        // if we have a text then report it
        if (msg.len > 0)
            PRINTK("%s ERROR [%i]%.*s\n", coroutine_context->caller, msg.len, msg.len, xenstore_dump);
        else
            PRINTK("%s ERROR\n", coroutine_context->caller);
        return -2;
    }

    // if the response is truncated then we have an error
    if (response && (msg.len > response_size))
    {
        PRINTK("%s truncated\n", coroutine_context->caller);
        return 1;
    }

    // we are successful
    return 0;
}

static int xenstore_read(const char *key, char *value, size_t value_size, size_t *value_length)
{
    int rc = 0;
    int key_length = strlen(key) + 1;
    struct xsd_sockmsg msg = { 0 };
    msg.type = XS_READ;
    msg.req_id = ++xenstore_req_id;
    msg.tx_id = 0;
    msg.len = key_length;

    // run this as a coroutine
    COROUTINE_DISPATCHER_BEGIN;
    if (((rc = xenstore_transact(&coroutine_context, &msg, sizeof(msg), NULL, 0, NULL)) != 0) ||
        ((rc = xenstore_transact(&coroutine_context, key, key_length, NULL, 0, NULL)) != 0))
    {
        PRINTK("Error writing message in %s", __FUNCTION__);
        return rc;
    }
    rc = xenstore_transact(&coroutine_context, NULL, 0, value, value_size, value_length);
    COROUTINE_DISPATCHER_END;

    if (rc < 0)
        *value_length = 0;

    return rc;
}

static int xenstore_get_perms(const char *path, char *value, size_t value_size, size_t *value_length)
{
    int rc = 0;
    int key_length = strlen(path) + 1;
    struct xsd_sockmsg msg = { 0 };
    msg.type = XS_GET_PERMS;
    msg.req_id = ++xenstore_req_id;
    msg.tx_id = 0;
    msg.len = key_length;

    // run this as a coroutine
    COROUTINE_DISPATCHER_BEGIN;
    if (((rc = xenstore_transact(&coroutine_context, &msg, sizeof(msg), NULL, 0, NULL)) != 0) ||
        ((rc = xenstore_transact(&coroutine_context, path, key_length, NULL, 0, NULL)) != 0))
    {
        PRINTK("Error writing message in %s", __FUNCTION__);
        return rc;
    }
    rc = xenstore_transact(&coroutine_context, NULL, 0, value, value_size, value_length);
    COROUTINE_DISPATCHER_END;

    if (rc < 0)
        *value_length = 0;

    return rc;
}

static int xenstore_set_perms(const char *path, const char *values)
{
    int rc = 0;
    int key_length = strlen(path) + 1;
    int value_length = strlen(values) + 1;
    struct xsd_sockmsg msg = { 0 };
    msg.type = XS_SET_PERMS;
    msg.req_id = ++xenstore_req_id;
    msg.tx_id = 0;
    msg.len = key_length + value_length;

    // run this as a coroutine
    COROUTINE_DISPATCHER_BEGIN;
    if (((rc = xenstore_transact(&coroutine_context, &msg, sizeof(msg), NULL, 0, NULL)) != 0) ||
        ((rc = xenstore_transact(&coroutine_context, path, key_length, NULL, 0, NULL)) != 0) ||
        ((rc = xenstore_transact(&coroutine_context, values, value_length, NULL, 0, NULL)) != 0))
    {
        PRINTK("Error writing message in %s", __FUNCTION__);
        return rc;
    }
    rc = xenstore_transact(&coroutine_context, NULL, 0, NULL, 0, NULL);
    COROUTINE_DISPATCHER_END;

    return rc;
}

/* Write a key/value pair to the XenStore */
static int xenstore_write(const char *key, const char *value)
{
    int rc = 0;
    int key_length = strlen(key) + 1;
    int value_length = strlen(value);
    struct xsd_sockmsg msg;
    msg.type = XS_WRITE;
    msg.req_id = ++xenstore_req_id;
    msg.tx_id = 0;
    msg.len = key_length + value_length;

    /* Write the message */
    COROUTINE_DISPATCHER_BEGIN;
    if (((rc = xenstore_transact(&coroutine_context, &msg, sizeof(msg), NULL, 0, NULL)) != 0) ||
        ((rc = xenstore_transact(&coroutine_context, key, key_length, NULL, 0, NULL)) != 0) ||
        ((rc = xenstore_transact(&coroutine_context, value, value_length, NULL, 0, NULL)) != 0))
    {
        PRINTK("Error writing message in %s", __FUNCTION__);
        return rc;
    }
    rc = xenstore_transact(&coroutine_context, NULL, 0, NULL, 0, NULL);
    COROUTINE_DISPATCHER_END;

    // success
    return rc;
}

/* make a subdirectory in the xenstore */
static int xenstore_mkdir(const char *directory)
{
    int rc = 0;
    int key_length = strlen(directory) + 1;

    struct xsd_sockmsg msg;
    msg.type = XS_MKDIR;
    msg.req_id = ++xenstore_req_id;
    msg.tx_id = 0;
    msg.len = key_length;

    /* Write the message */
    COROUTINE_DISPATCHER_BEGIN;
    if (((rc = xenstore_transact(&coroutine_context, &msg, sizeof(msg), NULL, 0, NULL)) != 0) ||
        ((rc = xenstore_transact(&coroutine_context, directory, key_length, NULL, 0, NULL)) != 0))
    {
        PRINTK("error sending key\n");
        return rc;
    }
    rc = xenstore_transact(&coroutine_context, NULL, 0, NULL, 0, NULL);
    COROUTINE_DISPATCHER_END;

    // success
    return rc;
}

/* Read a value from the store */
int xenstore_ls(const char * key, char *values, size_t value_size, size_t *value_length)
{
    int rc = 0;
    int key_length = strlen(key) + 1;
    struct xsd_sockmsg msg;
    msg.type = XS_DIRECTORY;
    msg.req_id = ++xenstore_req_id;
    msg.tx_id = 0;
    msg.len = key_length;

    COROUTINE_DISPATCHER_BEGIN;
    if (((rc = xenstore_transact(&coroutine_context, &msg, sizeof(msg), NULL, 0, NULL)) != 0) ||
        ((rc = xenstore_transact(&coroutine_context, key, key_length + 1, NULL, 0, NULL)) != 0))
    {
        PRINTK("error sending key\n");
        return rc;
    }
    rc = xenstore_transact(&coroutine_context, NULL, 0, values, value_size, value_length);
    COROUTINE_DISPATCHER_END;

    return rc;
}

static void xenstore_event_handler(evtchn_port_t port, struct pt_regs *regs, void *data)
{
    //struct xenstore_domain_interface *ring = xenstore_interface();
}

/**
 * Initialise the Xen store. The store ring buffer page is already mapped
 * into the guest address space, so we don't need to map the page into our
 * address space.
 *
 * @return 0 if success, -1 otherwise.
 */
int xenstore_init(void)
{
    if (!xenstore_event())
        return 0;

    // bind to the xenstore event channel
    port = xenevents_bind_channel(xenstore_event(), xenstore_event_handler);
    if (port == -1)
    {
        PRINTK("XEN store channel bind failed");
        return -1;
    }

    /* In case we have in-flight data after save/restore... */
    xenevents_notify_remote_via_evtchn(xenstore_event());

    // get my domain id
    size_t domid_length = 0;
    xenstore_read("domid", hypervisor_domid, sizeof(hypervisor_domid), &domid_length);
    hypervisor_domid[domid_length] = 0;
    xenconsole_printf("domid: %.*s\r\n", (int)domid_length, hypervisor_domid);

#if HYPERVISOR_PRODUCE_RING_BUFFER == 1
    // create buffer
    const size_t buffer_size = 100;
    size_t buffer_length = 0;
    char buffer[buffer_size + 1];
    char path[100];
    psnprintf(path, sizeof(path), "/local/domain/%s/data", hypervisor_domid);
    psnprintf(buffer, sizeof(buffer), "w%s", hypervisor_domid);
    xenstore_set_perms(path, buffer);
    xenstore_get_perms(path, buffer, buffer_size, &buffer_length);
    micropv_console_write("%s perms: %i %.*s\r\n", path, (int)buffer_length, (int)buffer_length, buffer);

    char data_path[100];
    char data_value[20];
    psnprintf(data_value, sizeof(data_value), "%lx", (long unsigned)0); //virt_to_mfn(ring_buffer));
    psnprintf(data_path, sizeof(data_path), "%s/command_line", path);
    if (xenstore_write(data_path, "100"))
        micropv_console_write("xenstore_write %s fails %s\r\n", data_path, xenstore_dump);
    else if (xenstore_read(data_path, buffer, buffer_size, &buffer_length))
        micropv_console_write("xenstore_read %s fails %s\r\n", data_path, xenstore_dump);
    else
        micropv_console_write("%s = %.*s\r\n", data_path, (int)buffer_length, buffer);
#endif

    return 0;
}

