/* ********************************************************************
   * Project   :
   * Author    : smartin
   ********************************************************************

    Modifications:
    0.01 29/11/2013 Initial version.
*/

/*---------------------------------------------------------------------
  -- macros (preamble)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/
#include <stdio.h>
#include <xenctrl.h>
#include <uuid/uuid.h>

/*---------------------------------------------------------------------
  -- project includes (import)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes (export)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- macros (postamble)
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

/*---------------------------------------------------------------------
  -- private functions
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- public functions
  ---------------------------------------------------------------------*/

int main(int argc, char *argv[], char *envp[])
{
    // check for parameters
    if (argc < 2)
    {
        printf("%s <dom uuid>\n", argv[0]);
        return -1;
    }
    printf("scheduling %s\n", argv[1]);

    // get the xen control interface
    xc_interface *xci = xc_interface_open(NULL, NULL, 0);
    if (!xci)
    {
        printf("error opening xen control interface\n");
        return -1;
    }

    /* initialize major frame and number of minor frames */
    struct xen_sysctl_arinc653_schedule sched = {0};
    sched.major_frame = 0;
    sched.num_sched_entries = 1;

    // initialise frames
    int i;
    for (i = 0; i < sched.num_sched_entries; ++i)
    {
        /* identify domain by UUID */
        if (uuid_parse(argv[1], sched.sched_entries[i].dom_handle))
        {
            printf("error parsing uuid %s\n", argv[1]);
            return -1;
        }

        /* must be 0 */
        sched.sched_entries[i].vcpu_id = 0;

        /* runtime in ms */
        sched.sched_entries[i].runtime = 1;

        /* updated major frame time */
        sched.major_frame += sched.sched_entries[i].runtime;
    }

    i = xc_sched_arinc653_schedule_set(xci, &sched);
    if (i)
    {
        printf("error %i setting scheduler data\n", i);
        return -1;
    }

    return 0;
}
