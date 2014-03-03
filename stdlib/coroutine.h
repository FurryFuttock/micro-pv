/* ********************************************************************
   * Project   :
   * Author    : smartin
   ********************************************************************

    Modifications:
    0.01 01/11/2013 Initial version.
*/

/*---------------------------------------------------------------------
  -- macros (preamble)
  ---------------------------------------------------------------------*/
#define COROUTINE_DECLARE(return_type, function_name, parameters...) return_type function_name(coroutine_context_t *coroutine_context, parameters)

#define COROUTINE_BEGIN             switch (coroutine_context->status) { case 0:;

#define COROUTINE_RETURN(x)             do { coroutine_context->status = __LINE__; return (x); case __LINE__:; } while(0)

#define COROUTINE_RETURNV            do { coroutine_context->status = __LINE__; case __LINE__:; } while(0)

#define COROUTINE_END                   break; default: PRINTK("unknown status %i", coroutine_context->status); }
#define COROUTINE_DISPATCHER_BEGIN  { coroutine_context_t coroutine_context = {0 , __FUNCTION__};

#define COROUTINE_DISPATCHER_END    }

#define COROUTINE_CALL(function_name, parameters...) \
    function_name(&coroutine_context, parameters)

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

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
typedef struct coroutine_context_t {
    int status;
    const char *caller;
} coroutine_context_t;

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

