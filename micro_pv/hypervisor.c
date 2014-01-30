/* ********************************************************************
   * Project   :
   * Author    : smartin
   ********************************************************************

    Talks to the XEN hypervisor

    Modifications:
    0.01 24/10/2013 Initial version.
*/

/*---------------------------------------------------------------------
  -- macros (preamble)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/
#include <stdint.h>
#include <xen/xen.h>
#include <xen/grant_table.h>
#include <xen/features.h>
#include <xen/version.h>

#include <string.h>

/*---------------------------------------------------------------------
  -- project includes (import)
  ---------------------------------------------------------------------*/
#include "hypercall.h"
#include "psnprintf.h"
#include "xenevents.h"
#include "xenconsole.h"
#include "xenstore.h"
#include "xentime.h"

/*---------------------------------------------------------------------
  -- project includes (export)
  ---------------------------------------------------------------------*/
#include "hypervisor.h"

/*---------------------------------------------------------------------
  -- macros (postamble)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/
static void hypervisor_setup_xen_features(void);

/**
 * Map a physical page into the machine memory space
 *
 * @param guest_address
 *               Page address in the virtual machine.
 * @param machine_address
 *               Page address in the physical memory.
 *
 * @return Pointer to the address of the page in the virtual machine.
 */
static void *hypervisor_map_page(unsigned long guest_address, unsigned long long machine_address);

#if HYPERVISOR_PRODUCE_RING_BUFFER == 1
/**
 * Publish a shared page.
 */
static void hypervisor_produce_ring_buffer();
#endif

#if HYPERVISOR_CONSUME_RING_BUFFER == 1
/**
 * Consume a page offered by another VM
 *
 * @param dom_friend
 * @param entry
 * @param shared_page
 * @param handle
 *
 * @return
 */
static grant_handle_t hypervisor_consume_ring_buffer(domid_t dom_friend, unsigned int entry, void *ring_buffer, grant_handle_t *handle);
#endif

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/
uint8_t xen_features[XENFEAT_NR_SUBMAPS * 32];

extern char shared_info[__PAGE_SIZE];
shared_info_t *hypervisor_shared_info;

#if HYPERVISOR_PRODUCE_RING_BUFFER == 1
grant_entry_v2_t grant_table[1];
#endif

/*---------------------------------------------------------------------
  -- local variables
  ---------------------------------------------------------------------*/
start_info_t hypervisor_start_info;

/*---------------------------------------------------------------------
  -- private functions
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- public functions
  ---------------------------------------------------------------------*/

void printk(const char *file, long line, const char *format, ...)
{
    va_list args;
    int message_length;

    // process the format once to get the string length
    va_start(args, format);
    message_length = pvsnprintf(NULL, 0, format, args);
    va_end(args);

    // if we have a string then print it
    if (message_length > 0)
    {
        // create the header
        char header[strlen(file)+ 10];
        int header_length = psnprintf(header, sizeof(header), "%s@%.5li: ", file, line);

        // create the output string
        char message[message_length + 1];
        va_start(args, format);
        message_length = pvsnprintf(message, message_length + 1, format, args);
        va_end(args);

        // send to the console
        HYPERVISOR_console_io(CONSOLEIO_write, header_length, header);
        HYPERVISOR_console_io(CONSOLEIO_write, message_length, message);

        // make sure that the line is terminated
        if (message[message_length - 1] != '\n')
        {
            static char lf = '\n';
            HYPERVISOR_console_io(CONSOLEIO_write, 1, &lf);
        }
    }
}

static void hypervisor_setup_xen_features(void)
{
    xen_feature_info_t fi;
    int i, j;

    for (i = 0; i < XENFEAT_NR_SUBMAPS; i++)
    {
        fi.submap = 0;
        fi.submap_idx = i;
        if (HYPERVISOR_xen_version(XENVER_get_features, &fi) < 0)
            break;

        for (j = 0; j < 32; j++) xen_features[i * 32 + j] = !!(fi.submap & 1 << j);
    }
}

void hypervisor_start(start_info_t *si)
{
    // dmesg print useful information
    PRINTK("Allocated memory pages           : %lu", si->nr_pages);
    PRINTK("Allocated memory MB              : %lu", si->nr_pages >> (20 - __PAGE_SHIFT));
    PRINTK("Machine address of shared memory : 0x%lx", si->shared_info);

    // initialise FPU
    __asm__ volatile(" fninit");

    // initialise sse
    unsigned long status = 0x1f80;
    __asm__ volatile("ldmxcsr %0" : : "m" (status));

    // store the startup information. This is passed in as a parameter to the _start function by the hypervisor.
    memcpy(&hypervisor_start_info, si, sizeof(hypervisor_start_info));

    // setup featers
    hypervisor_setup_xen_features();

    // initialise the traps
    hypervisor_trap_init();

    // let's map the shared info page into our memory map. Here we use the shared_info data area we defined
    // in bootstrap.<arch>.S
    hypervisor_shared_info = hypervisor_map_page((unsigned long)&shared_info, hypervisor_start_info.shared_info);

    // initialise the event interface
    xenevents_init();

    // initialise the console interface
    xenconsole_init();

    // initialise the time interface
    xentime_init();

#if HYPERVISOR_PRODUCE_RING_BUFFER == 1
    // let's offer up our transfer page
    hypervisor_produce_ring_buffer();
#endif

    // initialise the xenstore interface
    xenstore_init();
}

#if HYPERVISOR_CONSUME_RING_BUFFER == 1
grant_handle_t hypervisor_consume_ring_buffer(domid_t dom_friend, unsigned int entry, void *shared_page, grant_handle_t *handle)
{
    /* Set up the mapping operation */
    gnttab_map_grant_ref_t map_op;
    map_op.host_addr = (uint64_t)shared_page;
    map_op.flags = GNTMAP_host_map;
    map_op.ref = entry;
    map_op.dom = dom_friend;

    /* Perform the map */
    HYPERVISOR_grant_table_op(GNTTABOP_map_grant_ref, &map_op, 1);

    /* Check if it worked */
    if (map_op.status != GNTST_okay)
    {
        return -1;
    }
    else
    {
        /* Return the handle */
        *handle = map_op.handle;
        return 0;
    }
}
#endif

#if HYPERVISOR_PRODUCE_RING_BUFFER == 1
void hypervisor_produce_ring_buffer()
{
    uint16_t flags;
    /* Create the grant table */
    gnttab_setup_table_t setup_op;

    setup_op.dom = DOMID_SELF;
    setup_op.nr_frames = 1;
    setup_op.status = GNTST_okay;
    setup_op.frame_list = (void*)grant_table;

    HYPERVISOR_grant_table_op(GNTTABOP_setup_table, &setup_op, 1);

    /* Offer the grant */
    grant_table[0].domid = 0;
    grant_table[0].frame = (uint64_t)ring_buffer >> 12;
    flags = GTF_permit_access & GTF_reading & GTF_writing;
    grant_table[0].flags = flags;
}
#endif

void *hypervisor_map_page(unsigned long guest_address, unsigned long long machine_address)
{
    HYPERVISOR_update_va_mapping(guest_address, __pte(machine_address | 7), UVMF_INVLPG);
    return (void*)guest_address;
}

