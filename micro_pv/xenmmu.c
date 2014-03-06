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
#include "../micro_pv.h"
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

void *xenmmu_remap_page(uint64_t physical_address, uint64_t machine_address, int readonly)
{
    // Update the mapping. I have had problems using a readonly mapping, however I'm not sure whether that was to do with
    // this call, or the page that was being mapped.
    int rc = HYPERVISOR_update_va_mapping(physical_address, __pte(machine_address | (readonly ? L1_PROT_RO : L1_PROT)), UVMF_INVLPG);
    if (rc)
    {
        PRINTK("HYPERVISOR_update_va_mapping returns %i", rc);
        return NULL;
    }
    else
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
        xenmmu_remap_page(physical_address, mfn[fn] << L1_PAGETABLE_SHIFT, readonly);
    }

    // return a pointer to this buffer
    return physical_ptr;
}

