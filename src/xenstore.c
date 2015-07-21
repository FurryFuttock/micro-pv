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
#include "xenmmu.h"
#include "xenconsole.h"
#include "psnprintf.h"

/*---------------------------------------------------------------------
  -- project includes (export)
  ---------------------------------------------------------------------*/
#include "xenstore.h"

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#define XENSTORE_IO_ACTIVE 1

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
static uint32_t xenstore_flags = 0;
static char xenstore_dump[XENSTORE_RING_SIZE] = { 0 };
static char hypervisor_domid[6];
static evtchn_port_t port = -1;
static volatile int xenstore_event_fired = 0;
static xenbus_transaction_t current_xbt = 0;

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
    volatile XENSTORE_RING_IDX *head = &xenstore->req_cons;
    XENSTORE_RING_IDX tail = MASK_XENSTORE_IDX(xenstore->req_prod);

    int i;
    for (i = 0; i < length; i++)
    {
        // wait for space available
        XENSTORE_RING_IDX next_tail = MASK_XENSTORE_IDX(tail + 1);
        while (next_tail == *head)
            mb();

        // store data
        xenstore->req[tail] = message[i];
        tail = next_tail;
    }

    /* Ensure that the data really is in the ring before continuing */
    wmb();

    // update the index
    // XENSTORE doesn't really handle this as a circular buffer, as on ring roll over it
    // stalls if tail < req_prod
    xenstore->req_prod += length;

    // return success
    return 0;
}

/* Read a response from the response ring */
static int _xenstore_read_response(char * message, int length)
{
    struct xenstore_domain_interface *xenstore = xenstore_interface();
    volatile XENSTORE_RING_IDX *tail = &xenstore->rsp_prod;
    XENSTORE_RING_IDX head = MASK_XENSTORE_IDX(xenstore->rsp_cons);

    memset(message, 0xff, length);

    int i;
    for (i = 0; i < length; i++)
    {
        /* Wait for the back end put data in the buffer */
        while (head == MASK_XENSTORE_IDX(*tail))
            mb();

        // read data
        message[i] = xenstore->rsp[head];
        head = MASK_XENSTORE_IDX(head + 1);
    }

    // update the index
    xenstore->rsp_cons += length;

    return 0;
}

static int xenstore_read_response(char *response, size_t response_size, size_t *response_length)
{
    // make sure that the command was notified
    xenevents_notify_remote_via_evtchn(xenstore_event());

    // read the response header
    struct xsd_sockmsg msg = { 0 };
    _xenstore_read_response((char *)&msg, sizeof(msg));

    // read any response data
    if (msg.len > 0)
    {
        // read the requested response
        register size_t length = MIN(MASK_XENSTORE_IDX(msg.len), response_size);
        if (response_length)
            *response_length = length;
        _xenstore_read_response(response, length);

        // read the remainder
        if (response_size < msg.len)
        {
            _xenstore_read_response(xenstore_dump, msg.len - response_size);
            xenstore_dump[msg.len - response_size] = 0;
        }
    }

    // we are out of sync so fail
    if (msg.req_id != xenstore_req_id)
    {
        PRINTK("invalid request id. Sent=%i, Received=%i", xenstore_req_id, msg.req_id);
        return -1;
    }

    // report errors
    if (msg.type == XS_ERROR)
    {
        // if we have a text then report it
        if (msg.len > 0)
            PRINTK("ERROR [%i]%.*s", msg.len, msg.len, xenstore_dump);
        else
            PRINTK("ERROR no data");
        return -2;
    }

    // if the response is truncated then we have an error
    if (response && (msg.len > response_size))
    {
        PRINTK("ERROR truncated");
        return 1;
    }

    // we are successful
    return 0;
}

static int xenstore_request(xenbus_transaction_t xbt)
{
    // wait for previous request to finish
    micropv_interrupt_disable();
    while ((xenstore_flags & XENSTORE_IO_ACTIVE) || (xbt != current_xbt))
    {
        micropv_interrupt_enable();
        micropv_interrupt_disable();
    }
    xenstore_flags |= XENSTORE_IO_ACTIVE;
    micropv_interrupt_enable();

    // return the next id
    return ++xenstore_req_id;
}

static void xenstore_release(void)
{
    micropv_interrupt_disable();
    xenstore_flags &= ~XENSTORE_IO_ACTIVE;
    micropv_interrupt_enable();
}

int xenstore_read(xenbus_transaction_t xbt, const char *key, char *value, size_t value_size, size_t *value_length)
{
    int rc = 0;
    int key_length = strlen(key) + 1;
    struct xsd_sockmsg msg = { 0 };
    msg.req_id = xenstore_request(xbt);
    msg.type = XS_READ;
    msg.tx_id = xbt;
    msg.len = key_length;

    if (((rc = xenstore_write_request((char *)&msg, sizeof(msg))) != 0) ||
        ((rc = xenstore_write_request(key, key_length)) != 0))
    {
        PRINTK("Error writing message in %s", __FUNCTION__);
        goto fail;
    }
    rc = xenstore_read_response(value, value_size, value_length);

    if (rc < 0)
        *value_length = 0;

    fail:
        xenstore_release();

    return rc;
}

int xenstore_get_perms(xenbus_transaction_t xbt, const char *path, char *value, size_t value_size, size_t *value_length)
{
    int rc = 0;
    int key_length = strlen(path) + 1;
    struct xsd_sockmsg msg = { 0 };
    msg.type = XS_GET_PERMS;
    msg.req_id = xenstore_request(xbt);
    msg.tx_id = xbt;
    msg.len = key_length;

    if (((rc = xenstore_write_request((char *)&msg, sizeof(msg))) != 0) ||
        ((rc = xenstore_write_request(path, key_length)) != 0))
    {
        PRINTK("Error writing message in %s", __FUNCTION__);
        goto fail;
    }
    rc = xenstore_read_response(value, value_size, value_length);

    if (rc < 0)
        *value_length = 0;

    fail:
        xenstore_release();

    return rc;
}

int xenstore_set_perms(xenbus_transaction_t xbt, const char *path, const char *values)
{
    int rc = 0;
    int key_length = strlen(path) + 1;
    int value_length = strlen(values) + 1;
    struct xsd_sockmsg msg = { 0 };
    msg.type = XS_SET_PERMS;
    msg.req_id = xenstore_request(xbt);
    msg.tx_id = xbt;
    msg.len = key_length + value_length;

    if (((rc = xenstore_write_request((char *)&msg, sizeof(msg))) != 0) ||
        ((rc = xenstore_write_request(path, key_length)) != 0) ||
        ((rc = xenstore_write_request(values, value_length)) != 0))
    {
        PRINTK("Error writing message in %s", __FUNCTION__);
        goto fail;
    }
    rc = xenstore_read_response(NULL, 0, NULL);

    fail:
        xenstore_release();

    return rc;
}

void xenstore_wait_for_event()
{
    while (!xenstore_event_fired)
        micropv_scheduler_block();

    micropv_interrupt_disable();
    xenstore_event_fired = 0;
    micropv_interrupt_enable();
}

int xenstore_write(xenbus_transaction_t xbt, const char *key, const char *value)
{
    int rc = 0;
    int key_length = strlen(key) + 1;
    int value_length = strlen(value);
    struct xsd_sockmsg msg;
    msg.type = XS_WRITE;
    msg.req_id = xenstore_request(xbt);
    msg.tx_id = xbt;
    msg.len = key_length + value_length;

    PRINTK("WRITE %s - %s", key, value);

    /* Write the message */
    if (((rc = xenstore_write_request((char *)&msg, sizeof(msg))) != 0) ||
        ((rc = xenstore_write_request(key, key_length)) != 0) ||
        ((rc = xenstore_write_request(value, value_length)) != 0))
    {
        PRINTK("Error writing message in %s", __FUNCTION__);
        goto fail;
    }
    rc = xenstore_read_response(NULL, 0, NULL);

    fail:
        xenstore_release();

    return rc;
}

int xenstore_write_if_different(xenbus_transaction_t xbt, const char *key, const char *value)
{
    int rc = -1;
    int retry = 0;
    int local_transaction = 0;

    do
    {
        // if we don't have a transaction then start one
        if (xbt == XBT_NIL)
        {
            if (xenstore_transaction_start(&xbt))
                goto fail;
            local_transaction = 0;
        }

        char xenstore_value[strlen(value)+2];
        size_t xenstore_value_length;
        rc = xenstore_read(xbt, key, xenstore_value, sizeof(xenstore_value), &xenstore_value_length);
        if (!rc && strcmp(value, xenstore_value))
            rc = xenstore_write(xbt,key,value);

        // if the transaction is local to this function then commit
        if (local_transaction)
        {
            xenstore_transaction_end(xbt, rc, &retry);
            xbt = XBT_NIL;
        }
    } while (retry);

    fail:
        return rc;
}

int xenstore_mkdir(xenbus_transaction_t xbt, const char *directory)
{
    int rc = 0;
    int key_length = strlen(directory) + 1;

    struct xsd_sockmsg msg;
    msg.type = XS_MKDIR;
    msg.req_id = xenstore_request(xbt);
    msg.tx_id = xbt;
    msg.len = key_length;

    /* Write the message */
    if (((rc = xenstore_write_request((char *)&msg, sizeof(msg))) != 0) ||
        ((rc = xenstore_write_request(directory, key_length)) != 0))
    {
        PRINTK("error sending mkdir");
        goto fail;
    }
    rc = xenstore_read_response(NULL, 0, NULL);

    fail:
        xenstore_release();

    return rc;
}

int xenstore_ls(xenbus_transaction_t xbt, const char * key, char *values, size_t value_size, size_t *value_length)
{
    int rc = 0;
    int key_length = strlen(key) + 1;
    struct xsd_sockmsg msg;
    msg.type = XS_DIRECTORY;
    msg.req_id = xenstore_request(xbt);
    msg.tx_id = xbt;
    msg.len = key_length;

    if (((rc = xenstore_write_request((char *)&msg, sizeof(msg))) != 0) ||
        ((rc = xenstore_write_request(key, key_length)) != 0))
    {
        PRINTK("error sending ls");
        goto fail;
    }
    rc = xenstore_read_response(values, value_size, value_length);

    fail:
        xenstore_release();

    return rc;
}

int xenstore_transaction_start(xenbus_transaction_t *xbt)
{
    int rc = 0;

    char value[12];
    size_t value_length;

    const char *key = "";
    int key_length = 1;

    struct xsd_sockmsg msg;
    msg.type = XS_TRANSACTION_START;
    msg.req_id = xenstore_request(XBT_NIL);
    msg.tx_id = 0;
    msg.len = key_length;

    /* Write the message */
    if (((rc = xenstore_write_request((char *)&msg, sizeof(msg))) != 0) ||
        ((rc = xenstore_write_request(key, key_length)) != 0))
    {
        PRINTK("error sending transaction_start");
        goto fail;
    }
    rc = xenstore_read_response(value, sizeof(value), &value_length);

    *xbt = strtol(&value[1], NULL, 10);

    fail:
        xenstore_release();

    return rc;
}

int xenstore_transaction_end(xenbus_transaction_t xbt, int abort, int *retry)
{
    int rc = 0;

    const char *key = abort ? "F" : "T";
    int key_length = 2;

    struct xsd_sockmsg msg;
    msg.type = XS_TRANSACTION_START;
    msg.req_id = xenstore_request(xbt);
    msg.tx_id = xbt;
    msg.len = key_length;

    /* Write the message */
    if (((rc = xenstore_write_request((char *)&msg, sizeof(msg))) != 0) ||
        ((rc = xenstore_write_request(key, key_length)) != 0))
    {
        PRINTK("error sending transaction_end");
        goto fail;
    }
    rc = xenstore_read_response(NULL, 0, NULL);

    // repeat?
    *retry = (rc == -2) && msg.len && !strcmp("EAGAIN", xenstore_dump);

    fail:
        xenstore_release();

    return rc;
}

int xenstore_rm(xenbus_transaction_t xbt, const char *path)
{
    int rc = 0;
    int path_length = strlen(path) + 1;
    struct xsd_sockmsg msg;
    msg.type = XS_RM;
    msg.req_id = xenstore_request(xbt);
    msg.tx_id = xbt;
    msg.len = path_length;

    /* Write the message */
    if (((rc = xenstore_write_request((char *)&msg, sizeof(msg))) != 0) ||
        ((rc = xenstore_write_request(path, path_length)) != 0))
    {
        PRINTK("error sending RM");
        goto fail;
    }
    rc = xenstore_read_response(NULL, 0, NULL);

    fail:
        xenstore_release();

    return rc;
}

static void xenstore_event_handler(evtchn_port_t port, struct pt_regs *regs, void *data)
{
    micropv_interrupt_disable();
    xenstore_event_fired = 1;
    micropv_interrupt_enable();
}

int xenstore_init(void)
{
    if (!xenstore_event())
        return 0;

    // bind to the xenstore event channel
    port = xenevents_bind_handler(xenstore_event(), xenstore_event_handler);
    if (port == -1)
    {
        PRINTK("XEN store channel bind failed");
        return -1;
    }

    /* In case we have in-flight data after save/restore... */
    xenevents_notify_remote_via_evtchn(xenstore_event());

    // get my domain id
    size_t domid_length = 0;
    xenstore_read(XBT_NIL, "domid", hypervisor_domid, sizeof(hypervisor_domid), &domid_length);
    hypervisor_domid[domid_length] = 0;
    xenconsole_printf("domid: %.*s\r\n", (int)domid_length, hypervisor_domid);

    // make sure that we can write to the data directory in our xenstore branch
    const size_t buffer_size = 10;
    size_t buffer_length = 0;
    char buffer[buffer_size + 1];
    psnprintf(buffer, sizeof(buffer), "w%s", hypervisor_domid);
    xenstore_set_perms(XBT_NIL, "data", buffer);
    xenstore_get_perms(XBT_NIL, "data", buffer, buffer_size, &buffer_length);

    return 0;
}

int xenstore_write_integer(xenbus_transaction_t xbt, const char *path, int32_t value)
{
    int rc = -1;
    char data_value[20];

    psnprintf(data_value, sizeof(data_value), "%u", value);
    if (xenstore_write(xbt, path, data_value))
        PRINTK("xenstore_write %s fails %s", path, xenstore_dump);
    else
        rc = 0;

    return rc;
}

int xenstore_read_integer(xenbus_transaction_t xbt, const char *path, int32_t *value)
{
    int rc = -1;
    char data_value[12] = {0};
    size_t data_length = 0;

    // check this is there
    if (xenstore_read(xbt, path, data_value, sizeof(data_value) - 1, &data_length))
        PRINTK("xenstore_read %s fails %s", path, xenstore_dump);
    else
    {
        *value = strtol(data_value, NULL, 10);
        rc = 0;
    }

    return rc;
}

int micropv_is_shutdown(xenbus_transaction_t xbt)
{
    const char *path = "control/shutdown";
    char data_value[10] = {0};
    size_t data_length = 0;
    int rc = -1;

    // check this is there
    if (xenstore_read(xbt, path, data_value, sizeof(data_value), &data_length))
        PRINTK("xenstore_read %s fails %s", path, xenstore_dump);
    else
        rc = data_length > 0;

    return rc;
}

int micropv_is_ready(xenbus_transaction_t xbt)
{
    int32_t value = 0;

    if (xenstore_read_integer(xbt, "data/ready", &value))
        return -1;
    else
        return value;
}

