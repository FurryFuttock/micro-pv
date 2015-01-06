/*  ***********************************************************************
    * Project:
    * File: xenmmu.c
    * Author: smartin
    ***********************************************************************

    I've got rid of most of the memory management as we only implement adding memory, so we don't have to track anything,
    also we don't manage virtual memory so we don't need page tables or the like.

    Modifications
    0.00 3/4/2014 created
*/

/*---------------------------------------------------------------------
  -- macros (preamble)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/
#include <stdint.h>
#include <stdlib.h>

/*---------------------------------------------------------------------
  -- project includes (imports)
  ---------------------------------------------------------------------*/
#include "../micropv.h"
#include "hypercall.h"
#include "hypervisor.h"

/*---------------------------------------------------------------------
  -- project includes (exports)
  ---------------------------------------------------------------------*/
#include "xenmmu.h"

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
uint64_t *phys_to_machine_mapping;

/*---------------------------------------------------------------------
  -- local variables
  ---------------------------------------------------------------------*/
static uint64_t max_pfn = 0;

/*---------------------------------------------------------------------
  -- implementation
  ---------------------------------------------------------------------*/

void xenmmu_init(void)
{
    // set the initial top of memory
    max_pfn = hypervisor_start_info.nr_pages;

    /* set up minimal memory infos */
    phys_to_machine_mapping = (uint64_t *)hypervisor_start_info.mfn_list;
}

void *micropv_remap_page(uint64_t physical_address, uint64_t machine_address, size_t size, int readonly)
{
    int pte_type = readonly ? L1_PROT_RO : L1_PROT;

    // round up to a number of pages
    int pages = (size + __PAGE_SIZE - 1) >> __PAGE_SHIFT;

    // Update the mapping. I have had problems using a readonly mapping, however I'm not sure whether that was to do with
    // this call, or the page that was being mapped.
    int page;
    for (page = 0; page < pages; page++)
    {
        int rc = HYPERVISOR_update_va_mapping(physical_address + (page << __PAGE_SHIFT), __pte((machine_address  + (page << __PAGE_SHIFT)) | pte_type), UVMF_INVLPG);
        if (rc)
        {
            PRINTK("FAIL mapping physical address 0x%lx to machine address 0x%lx", physical_address + (page << __PAGE_SHIFT), machine_address + (page << __PAGE_SHIFT));
            PRINTK("HYPERVISOR_update_va_mapping returns %i", rc);
            return NULL;
        }
    }
    PRINTK("Physical address 0x%lx mapped to machine address 0x%lx for a length of %i pages", physical_address, machine_address, pages);
    return (void*)physical_address;
}

void *xenmmu_map_frames(uint64_t mfn[], size_t mfn_size, int readonly)
{
    // get a pointer to the current top of memory
    uint64_t physical_address = max_pfn << __PAGE_SHIFT;
    void *physical_ptr = (void *)physical_address;

    // update the memory mapping
    for (int fn = 0; fn < mfn_size; fn++, max_pfn++, physical_address += __PAGE_SIZE)
    {
        micropv_remap_page(physical_address, mfn[fn] << L1_PAGETABLE_SHIFT, __PAGE_SIZE, readonly);
    }

    // return a pointer to this buffer
    return physical_ptr;
}

uint64_t micropv_virtual_to_machine_address(uint64_t virtual_address)
{
    uint64_t offset = virtual_address & ((1 << L1_PAGETABLE_SHIFT) - 1);
    uint64_t physical_address = to_phys(virtual_address);
    uint64_t physical_frame_number = PFN_DOWN(physical_address);
    uint64_t machine_frame_number = pfn_to_mfn(physical_frame_number);
    return (machine_frame_number << L1_PAGETABLE_SHIFT) | offset;
}

uint64_t micropv_machine_to_virtual_address(uint64_t machine_address)
{
    uint64_t offset = machine_address  & ((1 << L1_PAGETABLE_SHIFT) - 1);
    uint64_t machine_frame_number = machine_address >> L1_PAGETABLE_SHIFT;
    uint64_t physical_frame_number = mfn_to_pfn(machine_frame_number);
    uint64_t physical_address = physical_frame_number << L1_PAGETABLE_SHIFT;
    uint64_t virtual_address = (uint64_t)to_virt(physical_address);
    return virtual_address | offset;
}

