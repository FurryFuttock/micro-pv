/*  ***********************************************************************
    * Project:
    * File: xengnttab.c
    * Author: smartin
    ***********************************************************************

    OK. THis is what I think is going on here! Must be confirmed.

    NOMENCALTURE:

    VM = virtual machine
    Machine Frame Number = This is the page address in physical RAM
    Pseudo-physical Frame Number = This is the page address in the VM
    mfn = Machine Frame Number
    pfn = Pseudo-physical Frame Number

    This is separated out into two phases: Initialisation and Granting.

    Phase 1 - Initialisation
    ------------------------

    The hypervisor assigns storage for an array of grant_entry_t.

    1.- Call HYPERVISOR_grant_table_op GNTTABOP_setup_table to assign the grant_entry_t table. I seem to remember seeing somewhere
        that there is a minimum of 4 pages required. The frame element of the setup table parameter will return the machine frame
        numbers of the allocated pages.

    2.- The Hypervisor will now assign the requested number of pages to the specified DOM_ID. These pages are an array of
        grant_entry_t. The number of available grant entries is (PAGES * PAGE_SIZE) / sizeof(grant_entry_t). Must check what
        version of the grant we are using as v1 and v2 have a different definition of grant_entry_t.

    3.- The pages assigned by the hypervisor must now be mapped into the VM address space as a contiguous bank of storage
        using HYPERVISOR_mmu_update (this must be contiguous as it is a continuous array of grant_entry_t). The mini-os
        implementation (which is based on the Linux implementation or vice-versa) uses the pages after max_pfn as available
        for virtual memory. These are pages that are not in the startup memory map. As far as I can see max_pfn is initialised as
        start_info.nr_pages (capped to the maximum allowed page number).

    Phase 2 - Granting
    ------------------

    The VM fills in entries in the grant_entry_t array.

    1.-


    Modifications
    0.00 3/3/2014 created
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

/*---------------------------------------------------------------------
  -- project includes (imports)
  ---------------------------------------------------------------------*/
#include "hypercall.h"
#include "../micro_pv.h"

/*---------------------------------------------------------------------
  -- project includes (exports)
  ---------------------------------------------------------------------*/
#include "xengnttab.h"

/*---------------------------------------------------------------------
  -- macros (postamble)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- forward declarations
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
static grant_entry_v2_t grant_table[MICROPV_SHARED_PAGES];

/*---------------------------------------------------------------------
  -- implementation
  ---------------------------------------------------------------------*/

grant_handle_t micropv_shared_memory_consume(domid_t dom_friend, unsigned int entry, void *shared_page, grant_handle_t *handle)
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

void micropv_shared_memory_publish(void *buffer, int readonly)
{
#if 0
    int dom = 0;
    uint64_t mfn = buffer;
    gnttab_grant_foreign_access( domBid, mfn, (readonly ? 1 : 0) );
#elif 0
    gnttab_grant_foreign_access
    uint16_t flags;
    uint64_t frames[1];

    /* Offer the grant */
    grant_table[0].domid = 0;
    grant_table[0].frame = (uint64_t)buffer >> 12;
    flags = GTF_permit_access & GTF_reading & GTF_writing;
    grant_table[0].flags = flags;
#endif
}

int xengnttab_init()
{
    /* Create the grant table */
    gnttab_setup_table_t setup_op;
    uint64_t table_frames[MICROPV_SHARED_PAGES];
    //uint64_t status_frames[MICROPV_SHARED_PAGES];
    setup_op.dom = DOMID_SELF;
    setup_op.nr_frames = MICROPV_SHARED_PAGES;
    set_xen_guest_handle(setup_op.frame_list, table_frames);
    int rc = HYPERVISOR_grant_table_op(GNTTABOP_setup_table, &setup_op, 1);
    if (!rc)
    {
        for (int i = 0; i < MICROPV_SHARED_PAGES; i++)
        {
            PRINTK("frame[%i]=%lx", i, table_frames[i]);
        }
    }

    return rc;
}
