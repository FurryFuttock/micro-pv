/* ********************************************************************
   * Project   :
   * Author    : smartin
   ********************************************************************

    Platform dependent processor structures

    Modifications:
    0.01 29/10/2013 Initial version.
*/

#ifndef __OS_H__
#define __OS_H__

/*---------------------------------------------------------------------
  -- macros (preamble)
  ---------------------------------------------------------------------*/
#define LOCK_PREFIX ""

#define wrmsr(msr,val1,val2) \
      __asm__ __volatile__("wrmsr" \
                           : /* no outputs */ \
                           : "c" (msr), "a" (val1), "d" (val2))

#define wrmsrl(msr,val) wrmsr(msr,(uint32_t)((uint64_t)(val)),((uint64_t)(val))>>32)

#define __pte(x) ((pte_t) { (x) } )

//#define rmb()  __asm__ __volatile__ ("lock; addl $0,0(%%esp)": : :"memory")
#define mb()    __asm__ __volatile__ ("mfence":::"memory")
#define rmb()   __asm__ __volatile__ ("lfence":::"memory")
#define wmb()   __asm__ __volatile__ ("sfence" ::: "memory") /* From CONFIG_UNORDERED_IO (linux) */

#define rdtscll(val) do { \
     unsigned int __a,__d; \
     __asm volatile("rdtsc" : "=a" (__a), "=d" (__d)); \
     (val) = ((unsigned long)__a) | (((unsigned long)__d)<<32); \
} while(0)

#define ADDR (*(volatile long *) addr)
#define smp_processor_id() 0

#define synch_test_bit(nr,addr) (__builtin_constant_p(nr) ? synch_const_test_bit((nr),(addr)) : synch_var_test_bit((nr),(addr)))

/* This is a barrier for the compiler only, NOT the processor! */
#define barrier() __asm__ __volatile__("": : :"memory")

#define xchg(ptr,v) ((__typeof__(*(ptr)))__xchg((unsigned long)(v),(ptr),sizeof(*(ptr))))
#define __xg(x) ((volatile long *)(x))

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/
#include <xen/xen.h>

/*---------------------------------------------------------------------
  -- project includes (import)
  ---------------------------------------------------------------------*/
#include "arch_limits.h"

/*---------------------------------------------------------------------
  -- project includes (export)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- macros (postamble)
  ---------------------------------------------------------------------*/
//-- MEMORY POINTERS
#define VIRT_START                 ((unsigned long)&_text)
#define to_virt(x)                 ((void *)((unsigned long)(x)+VIRT_START))
#define mfn_to_pfn(_mfn)           (machine_to_phys_mapping[(_mfn)])
#define mfn_to_virt(_mfn)          (to_virt(mfn_to_pfn(_mfn) << __PAGE_SHIFT))

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/
typedef unsigned long paddr_t;
typedef unsigned long maddr_t;

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/
extern char _text;

/*---------------------------------------------------------------------
  -- local variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- private functions
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- public functions
  ---------------------------------------------------------------------*/
static inline void synch_set_bit(int nr, volatile void * addr)
{
    __asm__ __volatile__ (
        "lock btsl %1,%0"
        : "=m" (ADDR) : "Ir" (nr) : "memory" );
}

static inline void synch_clear_bit(int nr, volatile void * addr)
{
    __asm__ __volatile__ (
        "lock btrl %1,%0"
        : "=m" (ADDR) : "Ir" (nr) : "memory" );
}

static inline int synch_const_test_bit(int nr, const volatile void * addr)
{
    return ((1UL << (nr & 31)) &
            (((const volatile unsigned int *) addr)[nr >> 5])) != 0;
}

static inline int synch_var_test_bit(int nr, volatile void * addr)
{
    int oldbit;
    __asm__ __volatile__ (
        "btl %2,%1\n\tsbbl %0,%0"
        : "=r" (oldbit) : "m" (ADDR), "Ir" (nr) );
    return oldbit;
}

static inline int synch_test_and_set_bit(int nr, volatile void * addr)
{
    int oldbit;
    __asm__ __volatile__ (
        "lock btsl %2,%1\n\tsbbl %0,%0"
        : "=r" (oldbit), "=m" (ADDR) : "Ir" (nr) : "memory");
    return oldbit;
}

static inline int synch_test_and_clear_bit(int nr, volatile void * addr)
{
    int oldbit;
    __asm__ __volatile__ (
        "lock btrl %2,%1\n\tsbbl %0,%0"
        : "=r" (oldbit), "=m" (ADDR) : "Ir" (nr) : "memory");
    return oldbit;
}

static inline unsigned long __xchg(unsigned long x, volatile void * ptr, int size)
{
        switch (size) {
                case 1:
                        __asm__ __volatile__("xchgb %b0,%1"
                                :"=q" (x)
                                :"m" (*__xg(ptr)), "0" (x)
                                :"memory");
                        break;
                case 2:
                        __asm__ __volatile__("xchgw %w0,%1"
                                :"=r" (x)
                                :"m" (*__xg(ptr)), "0" (x)
                                :"memory");
                        break;
                case 4:
                        __asm__ __volatile__("xchgl %k0,%1"
                                :"=r" (x)
                                :"m" (*__xg(ptr)), "0" (x)
                                :"memory");
                        break;
                case 8:
                        __asm__ __volatile__("xchgq %0,%1"
                                :"=r" (x)
                                :"m" (*__xg(ptr)), "0" (x)
                                :"memory");
                        break;
        }
        return x;
}

/**
 * set_bit - Atomically set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * This function is atomic and may not be reordered.  See __set_bit()
 * if you do not require the atomic guarantees.
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 */
static inline void set_bit(int nr, volatile void * addr)
{
    __asm__ __volatile__( LOCK_PREFIX
        "btsl %1,%0"
        :"=m" (ADDR)
        :"dIr" (nr) : "memory");
}

/**
 * clear_bit - Clears a bit in memory
 * @nr: Bit to clear
 * @addr: Address to start counting from
 *
 * clear_bit() is atomic and may not be reordered.  However, it does
 * not contain a memory barrier, so if it is used for locking purposes,
 * you should call smp_mb__before_clear_bit() and/or smp_mb__after_clear_bit()
 * in order to ensure changes are visible on other processors.
 */
static inline void clear_bit(int nr, volatile void * addr)
{
    __asm__ __volatile__( LOCK_PREFIX
        "btrl %1,%0"
        :"=m" (ADDR)
        :"dIr" (nr));
}

/**
 * __ffs - find first bit in word.
 * @word: The word to search
 *
 * Undefined if no bit exists, so code should check against 0 first.
 */
static inline unsigned long __ffs(unsigned long word)
{
        __asm__("bsfq %1,%0"
                :"=r" (word)
                :"rm" (word));
        return word;
}

static __inline__ paddr_t machine_to_phys(maddr_t machine)
{
        paddr_t phys = mfn_to_pfn(machine >> __PAGE_SHIFT);
        phys = (phys << __PAGE_SHIFT) | (machine & ~__PAGE_MASK);
        return phys;
}
#endif

