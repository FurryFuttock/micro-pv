/* ***********************************************************************
   * Project:
   * File: xengnttab.h
   * Author: smartin
   ***********************************************************************

    Modifications
    0.00 3/3/2014 created
*/

/*---------------------------------------------------------------------
  -- macros (preamble)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/
#include <xen/grant_table.h>

/*---------------------------------------------------------------------
  -- project includes (imports)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes (exports)
  ---------------------------------------------------------------------*/

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
int xengnttab_init();
grant_ref_t xengnttab_share(int remote_dom, const void *buffer, int readonly);
void xengnttab_unshare(grant_ref_t ref);

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- local variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- implementation
  ---------------------------------------------------------------------*/

