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
#include "xengnttab.h"
#include "xenmmu.h"
#include "xenschedule.h"

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

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/
uint8_t xen_features[XENFEAT_NR_SUBMAPS * 32];

extern char shared_info[__PAGE_SIZE];
shared_info_t *hypervisor_shared_info;

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

void micropv_printk(const char *file, long line, const char *format, ...)
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

    // setup featers -- this is used by the hypervisor trap (see bootstrap.???.S)
    hypervisor_setup_xen_features();

    // initialise the traps -- this is now safe because we have the xen features
    xentraps_init();

    // initialise memory management
    xenmmu_init();

    // let's map the shared info page into our memory map. Here we use the shared_info data area we defined
    // in bootstrap.<arch>.S
    hypervisor_shared_info = xenmmu_remap_page((unsigned long)&shared_info, hypervisor_start_info.shared_info, 0);
    BUG_ON(hypervisor_shared_info == NULL);

    // initialise the event interface -- activates the hypervisor callbacks
    xenevents_init();

    // initialise the console interface
    xenconsole_init();

    // initialise the time interface
    xentime_init();

    // initialise the xenstore interface
    xenstore_init();

    // initialise the mapped memory
    xengnttab_init();

    // initialise the scheduler
    xenscheduler_init();
}
