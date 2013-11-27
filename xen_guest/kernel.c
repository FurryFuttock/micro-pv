/* ********************************************************************
   * Project   :
   * Author    : smartin
   ********************************************************************

    Modifications:
    0.01 23/10/2013 Initial version.
*/

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/
#include <stdint.h>
#include <xen/xen.h>
#include <hypercall.h>
#include <string.h>

/*---------------------------------------------------------------------
  -- project includes (import)
  ---------------------------------------------------------------------*/
#include "hypervisor.h"
#include "xenconsole.h"
#include "xentime.h"

/*---------------------------------------------------------------------
  -- project includes (export)
  ---------------------------------------------------------------------*/
#include "kernel.h"

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/
char stack[2 * __STACK_SIZE];
uint64_t ticks_count = 0;

/*---------------------------------------------------------------------
  -- local variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- private functions
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- public functions
  ---------------------------------------------------------------------*/

/* Main kernel entry point, called by trampoline */
void start_kernel(start_info_t *si)
{
    // write something to the XEN dmesg log
    PRINTK("pc400 started");

    // initialise communications with the hypervisor
    hypervisor_start(si);

    // start the periodic timer


    struct timeval start = { 0 };
    struct timeval now = { 0 };
    while (1)
    {
        // wait a second
        do xentime_gettimeofday(&now, NULL);
        while ((now.tv_sec - start.tv_sec) < 1);

        // prompt
        xenconsole_write("timeofday=%02i:%02i:%02i - %lu\r\n", (int)(now.tv_sec / 3600) % 24, (int)(now.tv_sec / 60) % 60, (int)now.tv_sec % 60, ticks_count);
        ticks_count = 0;
        extern int64_t virq_latency_min_nsec, virq_latency_max_nsec, virq_min_nsec, virq_max_nsec;
        xenconsole_write("%li,%li,%li,%li\r\n", virq_latency_min_nsec, virq_latency_max_nsec, virq_min_nsec, virq_max_nsec);
        virq_latency_max_nsec = virq_max_nsec = 0;
        virq_latency_min_nsec = virq_min_nsec = 0xffffffff;

        start = now;
    }
}

