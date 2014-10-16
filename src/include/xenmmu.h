/* ***********************************************************************
   * Project:
   * File: xenmmu.h
   * Author: smartin
   ***********************************************************************

    Modifications
    0.00 3/4/2014 created
*/

/*---------------------------------------------------------------------
  -- macros (preamble)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes (imports)
  ---------------------------------------------------------------------*/
#include "arch_limits.h"

/*---------------------------------------------------------------------
  -- project includes (exports)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- macros (postamble)
  ---------------------------------------------------------------------*/
#define _PAGE_PRESENT  0x001ULL
#define _PAGE_RW       0x002ULL
#define _PAGE_USER     0x004ULL
#define _PAGE_PWT      0x008ULL
#define _PAGE_PCD      0x010ULL
#define _PAGE_ACCESSED 0x020ULL
#define _PAGE_DIRTY    0x040ULL
#define _PAGE_PAT      0x080ULL
#define _PAGE_PSE      0x080ULL
#define _PAGE_GLOBAL   0x100ULL

#define L1_PROT                     (_PAGE_PRESENT|_PAGE_RW|_PAGE_ACCESSED|_PAGE_USER)
#define L1_PROT_RO                  (_PAGE_PRESENT|_PAGE_ACCESSED|_PAGE_USER)
#define L2_PROT                     (_PAGE_PRESENT|_PAGE_RW|_PAGE_ACCESSED|_PAGE_DIRTY|_PAGE_USER)
#define L3_PROT                     (_PAGE_PRESENT|_PAGE_RW|_PAGE_ACCESSED|_PAGE_DIRTY|_PAGE_USER)
#define L4_PROT                     (_PAGE_PRESENT|_PAGE_RW|_PAGE_ACCESSED|_PAGE_DIRTY|_PAGE_USER)

#define L1_PAGETABLE_SHIFT          12
#define L2_PAGETABLE_SHIFT          21
#define L3_PAGETABLE_SHIFT          30
#define L4_PAGETABLE_SHIFT          39

#define L1_PAGETABLE_ENTRIES    512
#define L2_PAGETABLE_ENTRIES    512
#define L3_PAGETABLE_ENTRIES    512
#define L4_PAGETABLE_ENTRIES    512

#define L1_MASK                     ((1UL << L2_PAGETABLE_SHIFT) - 1)
#define L2_MASK                     ((1UL << L3_PAGETABLE_SHIFT) - 1)
#define L3_MASK                     ((1UL << L4_PAGETABLE_SHIFT) - 1)

/* Given a virtual address, get an entry offset into a page table. */
#define l1_table_offset(_a) \
  (((_a) >> L1_PAGETABLE_SHIFT) & (L1_PAGETABLE_ENTRIES - 1))
#define l2_table_offset(_a) \
  (((_a) >> L2_PAGETABLE_SHIFT) & (L2_PAGETABLE_ENTRIES - 1))
#define l3_table_offset(_a) \
  (((_a) >> L3_PAGETABLE_SHIFT) & (L3_PAGETABLE_ENTRIES - 1))
#define l4_table_offset(_a) \
  (((_a) >> L4_PAGETABLE_SHIFT) & (L4_PAGETABLE_ENTRIES - 1))

#define VIRT_START                  ((unsigned long)&_text)

#define to_phys(x)                  ((unsigned long)(x)-VIRT_START)
#define to_virt(x)                  ((void *)((unsigned long)(x)+VIRT_START))
#define mfn_to_pfn(_mfn)            (machine_to_phys_mapping[(_mfn)])
#define mfn_to_virt(_mfn)           (to_virt(mfn_to_pfn(_mfn) << __PAGE_SHIFT))
#define PFN_DOWN(x)                 ((x) >> L1_PAGETABLE_SHIFT)
#define virt_to_pfn(_virt)          (PFN_DOWN(to_phys(_virt)))
#define pfn_to_mfn(_pfn)            (phys_to_machine_mapping[(_pfn)])
#define virt_to_mfn(_virt)          (pfn_to_mfn(virt_to_pfn(_virt)))

/*---------------------------------------------------------------------
  -- forward declarations
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/
typedef unsigned long paddr_t;
typedef unsigned long maddr_t;

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/

void xenmmu_init(void);

/**
 * Map an array of machine pages into a contiguous set of pages. See the
 * caveats in the xenmmu_remap_page for restrictions.
 *
 * @param mfn      Array of machine frame numbers.
 * @param mfn_size Number of elements in the frame number array.
 * @param readonly Page access privilege
 *
 * @return Pointer to the VM memory area if successfull, otherwise NULL.
 * @see xenmmu_remap_page
 */
void *xenmmu_map_frames(uint64_t mfn[], size_t mfn_size, int readonly);

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/
extern unsigned long *phys_to_machine_mapping;
extern char _text;

/*---------------------------------------------------------------------
  -- local variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- implementation
  ---------------------------------------------------------------------*/

static __inline__ paddr_t machine_to_phys(maddr_t machine)
{
        paddr_t phys = mfn_to_pfn(machine >> __PAGE_SHIFT);
        phys = (phys << __PAGE_SHIFT) | (machine & ~__PAGE_MASK);
        return phys;
}

