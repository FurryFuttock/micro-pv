/* ********************************************************************
   * Project   :
   * Author    : smartin
   ********************************************************************

    Modifications:
    0.01 7/7/2014 Initial version.
*/

/*---------------------------------------------------------------------
  -- macros (pre)
  ---------------------------------------------------------------------*/
#define PCI_DEVFN(slot, func) ((((slot) & 0x1f) << 3) | ((func) & 0x07))

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <xen/io/pciif.h>
#include <xen/io/xenbus.h>

/*---------------------------------------------------------------------
  -- project includes (import)
  ---------------------------------------------------------------------*/
#include "xenstore.h"
#include "xengnttab.h"
#include "xenevents.h"
#include "../micropv.h"

/*---------------------------------------------------------------------
  -- project includes (export)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- macros (post)
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
static volatile unsigned long pci_flags = 0;

/*---------------------------------------------------------------------
  -- private functions
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- public functions
  ---------------------------------------------------------------------*/

static void pci_event_handler(evtchn_port_t port, struct pt_regs *register_file, void *data)
{
	PRINTK("pci_event");
	set_bit(0, &pci_flags);
}

void micropv_pci_unmap_bus(micropv_pci_handle_t *handle)
{
	// disconnect from hypervisor
	char frontpath[64];
	snprintf(frontpath, sizeof(frontpath), "%s/state", handle->bus->nodename);
	char backpath[64];
	snprintf(backpath, sizeof(backpath), "%s/state", handle->bus->backend_path);

	char value[32];
	snprintf(value, sizeof(frontpath), "%u", XenbusStateClosing);
	xenstore_write_if_different(XBT_NIL, frontpath, value);

	int state;
	xenstore_read_integer(XBT_NIL, backpath, &state);
	while (state < XenbusStateClosing)
	{
		xenstore_wait_for_event();
		xenstore_read_integer(XBT_NIL, backpath, &state);
	}

	snprintf(value, sizeof(value), "%u", XenbusStateClosed);
	xenstore_write_if_different(XBT_NIL, frontpath, value);

	xenstore_read_integer(XBT_NIL, backpath, &state);
	while (state < XenbusStateClosed)
	{
		xenstore_wait_for_event();
		xenstore_read_integer(XBT_NIL, backpath, &state);
	}

	//snprintf(value, sizeof(value), "%u", XenbusStateInitialising);
	//xenstore_write_if_different(XBT_NIL, frontpath, value);
	//
	//xenstore_read_integer(XBT_NIL, backpath, &state);
	//while ((state < XenbusStateInitWait) || (state >= XenbusStateClosed))
	//{
	//	xenstore_wait_for_event();
	//	xenstore_read_integer(XBT_NIL, backpath, &state);
	//}

	snprintf(frontpath, sizeof(frontpath), "%s/info-ref", handle->bus->nodename);
	xenstore_rm(XBT_NIL, frontpath);

	snprintf(frontpath, sizeof(frontpath), "%s/event-channel", handle->bus->nodename);
	xenstore_rm(XBT_NIL, frontpath);

	// cleanup data
	handle->status = 0;
	if (handle->bus->grant_ref != -1)
	{
		xengnttab_unshare(handle->bus->grant_ref);
		handle->bus->grant_ref = 0;
	}
	if (handle->bus->port != -1)
	{
		xenevents_unbind_channel(handle->bus->port);
		handle->bus->port = -1;
		handle->bus->channel = -1;
	}
}

int micropv_pci_map_bus(micropv_pci_handle_t *handle)
{
	int retry;
	xenbus_transaction_t xbt = 0;

	// set default values so we can cleanup OK
	handle->status = micropv_pci_initialisation_none;
	handle->bus->backend_domain = -1;
	handle->bus->grant_ref = -1;
	handle->bus->port = handle->bus->channel = -1;

	char value[32];
	char path[64];
	snprintf(path, sizeof(path), "%s/backend-id", handle->bus->nodename);
	if (xenstore_read_integer(XBT_NIL, path, &handle->bus->backend_domain))
		goto fail;

	if (xenevents_alloc_channel(handle->bus->backend_domain, &handle->bus->channel))
		goto fail;
	handle->bus->port = xenevents_bind_channel(handle->bus->channel, pci_event_handler);

	memset(handle->bus->page_buffer, 0, 4096);
	handle->bus->grant_ref = xengnttab_share(handle->bus->backend_domain, handle->bus->page_buffer, 0);

	do
	{
		if (xenstore_transaction_start(&xbt))
			goto fail;

		snprintf(path, sizeof(path), "%s/pci-op-ref", handle->bus->nodename);
		snprintf(value, sizeof(path), "%u", handle->bus->grant_ref);
		if (xenstore_write(xbt, path, value))
			goto fail;

		snprintf(path, sizeof(path), "%s/event-channel", handle->bus->nodename);
		snprintf(value, sizeof(path), "%u", handle->bus->channel);
		if (xenstore_write(xbt, path, value))
			goto fail;

		snprintf(path, sizeof(path), "%s/magic", handle->bus->nodename);
		if (xenstore_write(xbt, path, XEN_PCI_MAGIC))
			goto fail;

		snprintf(path, sizeof(path), "%s/state", handle->bus->nodename);
		snprintf(value, sizeof(path), "%u", XenbusStateInitialised);
		if (xenstore_write_if_different(xbt, path, value))
			goto fail;

		xenstore_transaction_end(xbt, 0, &retry);
	} while (retry);

	size_t value_length = 0;
	snprintf(path, sizeof(path), "%s/backend", handle->bus->nodename);
	if (xenstore_read(XBT_NIL, path, handle->bus->backend_path, sizeof(handle->bus->backend_path), &value_length))
		goto fail;

	snprintf(path, sizeof(path), "%s/state", handle->bus->backend_path);
	int state;
	if (xenstore_read_integer(XBT_NIL, path, &state))
		goto fail;
	while (state < XenbusStateConnected)
	{
		xenstore_wait_for_event();
		if (xenstore_read_integer(XBT_NIL, path, &state))
			goto fail;
	}

	snprintf(path, sizeof(path), "%s/state", handle->bus->nodename);
	snprintf(value, sizeof(path), "%u", XenbusStateConnected);
	if (xenstore_write_if_different(xbt, path, value))
		goto fail;

	// if we get here then we have initialised correctly
	handle->status = 1;

	return 0;

	fail:
		if (xbt)
			xenstore_transaction_end(xbt, 1, &retry);
		micropv_pci_unmap_bus(handle);

		return -1;
}

void pci_op(micropv_pci_handle_t *handle, struct xen_pci_op *op)
{
	struct xen_pci_sharedinfo *info = (struct xen_pci_sharedinfo *)handle->bus->page_buffer;

    info->op = *op;
    /* Make sure info is written before the flag */
    wmb();
	clear_bit(0, &pci_flags);
    set_bit(_XEN_PCIF_active, (void*) &info->flags);
    xenevents_notify_remote_via_evtchn(handle->bus->channel);

    while (!test_bit(0, &pci_flags) || test_bit(_XEN_PCIF_active, (void*) &info->flags));

    /* Make sure flag is read before info */
    rmb();
    *op = info->op;
}

int pci_conf_read(micropv_pci_handle_t *handle, micropv_pci_device_t *device,
				  unsigned int off, unsigned int size, unsigned int *val)
{
	struct xen_pci_op op;

	op.cmd = XEN_PCI_OP_conf_read;
	op.domain = device->domain;
	op.bus = device->bus;
	op.devfn = PCI_DEVFN(device->slot, device->fun);
	op.offset = off;
	op.size = size;

	pci_op(handle, &op);

	if (op.err)
		return op.err;

	*val = op.value;

	return 0;
}

int micropv_pci_scan_bus(micropv_pci_handle_t *handle)
{
	char path[strlen(handle->bus->backend_path) + 1 + 5 + 10 + 1];

	snprintf(path, sizeof(path), "%s/num_devs", handle->bus->backend_path);
	int num_devs = 0;
	xenstore_read_integer(XBT_NIL, path, &num_devs);

	int d;
	for (d = 0; (d < num_devs); d++)
	{
		char dev[15];
		size_t dev_length;
		snprintf(path, sizeof(path), "%s/vdev-%d", handle->bus->backend_path, d);
		xenstore_read(XBT_NIL, path, dev, sizeof(dev), &dev_length);

		char *end, *start = dev;
		micropv_pci_device_t device;
		device.domain = strtol(start, &end, 16); start = end + 1;
		device.bus    = strtol(start, &end, 16); start = end + 1;
		device.slot   = strtol(start, &end, 16); start = end + 1;
		device.fun    = strtol(start, &end, 16);

		pci_conf_read(handle, &device, 0x00, 2, &device.vendor);
		pci_conf_read(handle, &device, 0x02, 2, &device.device);
		pci_conf_read(handle, &device, 0x08, 1, &device.rev);
		pci_conf_read(handle, &device, 0x0a, 2, &device.class);

		PRINTK("%04x:%02x:%02x.%02x %04x: %04x:%04x (rev %02x)",
			   device.domain, device.bus, device.slot, device.fun,
			   device.class, device.vendor, device.device, device.rev);

		int i;
		for (i = 0; i < SIZEOF_ARRAY(device.bar); i++)
		{
			pci_conf_read(handle, &device, 0x10 + (i << 2), 4, &device.bar[i].raw);
			if (device.bar[i].io.type)
				PRINTK("BAR %i: Type=IO Address=%x", i, device.bar[i].io.address);
			else
				PRINTK("BAR %i: Type=Memory Locatable=%x Prefetchable=%x Address=%x", i, device.bar[i].memory.locatable, device.bar[i].memory.prefretchable, device.bar[i].memory.address);
		}

		for (i = 0; (i < handle->bus->probes) && !handle->dispatcher; i++)
			handle->dispatcher = handle->bus->probe[i](handle, &device);
	}

	return 0;
}

