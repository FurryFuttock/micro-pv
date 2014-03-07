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

    #### TODO ####
    In the Linux implementation grant_entry_t AND grant_status_t arrays are created and mapped into VM memory.
    Not sure what is going on here though.
    #### TODO ####

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

    The VM fills in entries in the grant_entry_t array to produce the share.

    1.- The frame number and domid are written to the grant_entry_t

    2.- The flags are written. The page will be shared once the flags are written.

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
#include "../micropv.h"
#include "xenmmu.h"
#include "xenevents.h"
#include "xenstore.h"

/*---------------------------------------------------------------------
  -- project includes (exports)
  ---------------------------------------------------------------------*/
#include "xengnttab.h"

/*---------------------------------------------------------------------
  -- macros (postamble)
  ---------------------------------------------------------------------*/
#define NR_RESERVED_ENTRIES 8
#define NR_GRANT_FRAMES 4
#define NR_GRANT_ENTRIES_V1 (NR_GRANT_FRAMES * __PAGE_SIZE / sizeof(grant_entry_v1_t))
#define NR_GRANT_ENTRIES_V2 (NR_GRANT_FRAMES * __PAGE_SIZE / sizeof(grant_entry_v2_t))

/*---------------------------------------------------------------------
  -- forward declarations
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/
typedef grant_ref_t uint32_t;

typedef struct grant_interface_t
{
    void (*xengnttab_share)(grant_ref_t ref, uint64_t mfn, int readonly);
    void (*xengnttab_list)();
    const grant_ref_t nr_grant_entries;
} grant_interface_t;

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/
//--- VERSION 1
static void xengnttab_share_v1(grant_ref_t ref, uint64_t mfn, int readonly);
static void xengnttab_list_v1();

//--- VERSION 2
static void xengnttab_share_v2(grant_ref_t ref, uint64_t mfn, int readonly);
static void xengnttab_list_v2();

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- local variables
  ---------------------------------------------------------------------*/
static char grant_table_pages[NR_GRANT_FRAMES][__PAGE_SIZE] __attribute__((aligned(__PAGE_SIZE))) = {{0}};

// grant_entry_v2_t is bigger that grant_entry_v1_t, so we get more grant_entry_v1_t entries
// hence we use the larger value for this ref list
static grant_ref_t gnttab_list[NR_GRANT_ENTRIES_V1] = {0};

static grant_interface_t interface_v1 =
{
    .xengnttab_share = xengnttab_share_v1,
    .xengnttab_list = xengnttab_list_v1,
    .nr_grant_entries = NR_GRANT_ENTRIES_V1,
};

static grant_interface_t interface_v2 =
{
    .xengnttab_share = xengnttab_share_v2,
    .xengnttab_list = xengnttab_list_v2,
    .nr_grant_entries = NR_GRANT_ENTRIES_V2,
};

static grant_interface_t *interface = NULL;

/*---------------------------------------------------------------------
  -- implementation
  ---------------------------------------------------------------------*/

static void put_free_entry(grant_ref_t ref)
{
    micropv_interrupt_disable();
    gnttab_list[ref] = gnttab_list[0];
    gnttab_list[0]  = ref;
    micropv_interrupt_enable();
}

static grant_ref_t get_free_entry(void)
{
    unsigned int ref;

    micropv_interrupt_disable();
    ref = gnttab_list[0];
    BUG_ON((ref < NR_RESERVED_ENTRIES) || (ref >= interface->nr_grant_entries));
    gnttab_list[0] = gnttab_list[ref];
    micropv_interrupt_enable();

    return ref;
}

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

static void xengnttab_share_v1(grant_ref_t ref, uint64_t mfn, int readonly)
{
    // set the grant data
    grant_entry_v1_t *gnttab_table = (grant_entry_v1_t *) grant_table_pages;
    gnttab_table[ref].frame = mfn;
    gnttab_table[ref].domid = 0;
    wmb();
    gnttab_table[ref].flags = GTF_permit_access | (readonly ? GTF_readonly : 0);

}

static void xengnttab_share_v2(grant_ref_t ref, uint64_t mfn, int readonly)
{
    // set the grant data
    grant_entry_v2_t *gnttab_table = (grant_entry_v2_t *) grant_table_pages;
    gnttab_table[ref].full_page.frame = mfn;
    gnttab_table[ref].full_page.hdr.domid = 0;
    wmb();
    gnttab_table[ref].full_page.hdr.flags = GTF_permit_access | (readonly ? GTF_readonly : 0);

}

void micropv_shared_memory_publish(const char *name, const void *buffer, int readonly)
{
    // convert the buffer address to machine frame number
    uint64_t mfn = virt_to_mfn(buffer);

    // get the index of the next available grant entry
    grant_ref_t ref = get_free_entry();

    // map the frame
    BUG_ON(interface == NULL);
    interface->xengnttab_share(ref, mfn, readonly);
    PRINTK("physical_address=%p mapped to machine_address=%lx with grant_ref=%i\n", buffer, mfn << __PAGE_SHIFT, ref);

    // publish this shared page in the xenstore
    xenstore_publish(name, ref);
}

static void xengnttab_list_v1()
{
    grant_entry_v1_t *gnttab_table = (grant_entry_v1_t *) grant_table_pages;
    for (int ref = 0; ref < interface->nr_grant_entries; ref++)
        if (gnttab_table[ref].flags)
            PRINTK("ref=%i, frame=%i, dom=%i, flags=%i\n", ref, gnttab_table[ref].frame, gnttab_table[ref].domid, gnttab_table[ref].flags);
}

static void xengnttab_list_v2()
{
    grant_entry_v2_t *gnttab_table = (grant_entry_v2_t *) grant_table_pages;
    for (int ref = 0; ref < interface->nr_grant_entries; ref++)
        if (gnttab_table[ref].full_page.hdr.flags)
            PRINTK("ref=%i, frame=%lii, dom=%i, flags=%i\n", ref, gnttab_table[ref].full_page.frame, gnttab_table[ref].full_page.hdr.domid, gnttab_table[ref].full_page.hdr.flags);
}

void micropv_shared_memory_list()
{
    BUG_ON(interface == NULL);
    interface->xengnttab_list();
}

int xengnttab_init()
{
    // get the grant version
    gnttab_get_version_t gnttab_version = {0};
    gnttab_version.dom = DOMID_SELF;
    int rc = HYPERVISOR_grant_table_op(GNTTABOP_get_version, &gnttab_version, 1);
    BUG_ON(rc != 0);
    PRINTK("We are using grant version %i\n", gnttab_version.version);

    // select the interface for this version
    switch (gnttab_version.version)
    {
    case 1: interface = &interface_v1; break;
    case 2: interface = &interface_v2; break;
    }

    // initialise the grant access list
    for (int i = NR_RESERVED_ENTRIES; i < interface->nr_grant_entries; i++)
        put_free_entry(i);

    /* Create the grant table */
    gnttab_setup_table_t setup_op;
    uint64_t table_frames[NR_GRANT_FRAMES] = {0};
    setup_op.dom = DOMID_SELF;
    setup_op.nr_frames = NR_GRANT_FRAMES;
    set_xen_guest_handle(setup_op.frame_list, table_frames);
    rc = HYPERVISOR_grant_table_op(GNTTABOP_setup_table, &setup_op, 1);
    if (!rc)
    {
        // log what we've found
        for (int i = 0; i < NR_GRANT_FRAMES; i++)
        {
            PRINTK("frame[%i]=%lx mapped to %p", i, table_frames[i], grant_table_pages[i]);
            void *rc = xenmmu_remap_page((uint64_t)grant_table_pages[i], table_frames[i] << __PAGE_SHIFT, 0);
            BUG_ON(rc == NULL);
        }
    }

    // perform the version initialisation
    return rc;
}

