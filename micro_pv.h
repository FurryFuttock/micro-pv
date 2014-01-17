/* ********************************************************************
   * Project   :
   * Author    : smartin
   ********************************************************************

    Modifications:
    0.01 13/01/2014 Initial version.
*/

/*---------------------------------------------------------------------
  -- macros (preamble)
  ---------------------------------------------------------------------*/
#define PRINTK(format...) printk(__FILE__, __LINE__, ##format)

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/
#include <stdint.h>
#include <sys/time.h>

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

/**
 * This is the processor register file. I thought about leaving
 * this as an opaque structure, however I think that it is more
 * useful publishing this so that the overlying operating system
 * can do whatever it wants.
 */
struct pt_regs {
        unsigned long r15;
        unsigned long r14;
        unsigned long r13;
        unsigned long r12;
        unsigned long rbp;
        unsigned long rbx;
/* arguments: non interrupts/non tracing syscalls only save upto here*/
        unsigned long r11;
        unsigned long r10;
        unsigned long r9;
        unsigned long r8;
        unsigned long rax;
        unsigned long rcx;
        unsigned long rdx;
        unsigned long rsi;
        unsigned long rdi;
        unsigned long orig_rax;
/* end of arguments */
/* cpu exception frame or undefined */
        unsigned long rip;
        unsigned long cs;
        unsigned long eflags;
        unsigned long rsp;
        unsigned long ss;
/* top of stack page */
};

enum xen_register_file
{
        xen_register_file_r15, xen_register_file_r14, xen_register_file_r13, xen_register_file_r12, xen_register_file_rbp, xen_register_file_rbx,
/* arguments: non interrupts/non tracing syscalls only save upto here*/
        xen_register_file_r11, xen_register_file_r10, xen_register_file_r9, xen_register_file_r8, xen_register_file_rax, xen_register_file_rcx,
        xen_register_file_rdx, xen_register_file_rsi, xen_register_file_rdi, xen_register_file_orig_rax,
/* end of arguments */
/* cpu exception frame or undefined */
        xen_register_file_rip, xen_register_file_cs, xen_register_file_eflags, xen_register_file_rsp, xen_register_file_ss,
        xen_register_file_registers
};

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/

// --------- TIME FUNCTIONS
/**
 * Initialise a stack context. This is analogous to thread.
 *
 * @param pt_regs    The processor register file to be initialised.
 * @param start_ptr  Pointer to function that will be run in this context.
 * @param stack_ptr  Pointer to the memory area for the stack for this context.
 * @param stack_size Size of the stack. For x86 platforms the stack grows down so we
 *                   have to initialise the stack pointer to stack_ptr + stack_size.
 */
void xentime_initialise_context(struct pt_regs *regs, void *start_ptr, void *stack_ptr, int stack_size);

/**
 * Return the current time of day. This is real time, not machine time,
 * and is kept in sync with the HyperVisor.
 *
 * @param tv     Output buffer to receive the current time.
 * @param tz     Timezone, this is currently ignored and only kept for compatibility.
 *
 * @return 0 if successful.
 */
int xentime_gettimeofday(struct timeval *tv, void *tz);

/**
 * Nanosecond timer. Guaranteed? to always increment.
 *
 * @return nano seconds since start of epoch
 */
uint64_t xentime_monotonic_clock(void);

// --------- CONSOLE IO FUNCTIONS

/**
 * Kernel print routine. Whatever is written here is available in the
 * Xen dmesg log for this VM.
 *
 * @param file   The name of the source file from which this function is called.
 *               Use the builtin __FILE__ macro.
 * @param line   The line in the file where this function is called from. Use the
 *               builtin __LINE__ macro.
 * @param format The standard print format to be applied to the following parameters
 */
void printk(const char *file, long line, const char *format, ...) __attribute__((format (printf, 3, 4)));

/**
 * Check whether there is anything in the Xen console read buffer that can be read.
 *
 * @return The number of bytes available to be read.
 */
int xenconsole_read_available();

/**
 * Check whether there is room available in the Xen console transmit buffer to send data.
 *
 * @return The number of bytes available to be written.
 */
int xenconsole_write_available();

/**
 * Read the available bytes in the buffer. This DOES NOT block, if there
 * are no bytes to read the the function returns immediately.
 *
 * @param ptr    The output buffer to receive the data.
 * @param len    The length of the buffer to receive the data.
 *
 * @return The number of bytes written to the output buffer.
 */
int xenconsole_read(void *ptr,  size_t len);

/**
 * Write the data from the input buffer to the Xen console buffer. This
 * DOES block, if there are not enough bytes for the complete register
 * to be written then it will wait for more to come available.
 *
 * @param ptr    The input buffer that will be written to the Xen console.
 * @param len    The length of the input buffer to bw written.
 *
 * @return The number of bytes written to the console.
 */
int xenconsole_write(const void *ptr, size_t len);

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/
/**
 * This is the register where the overlying operating system must store the
 * pointer to its' timer interrupt handler. I assume that this is where
 * the multitasking context switch is going to occur.
 *
 * @param pt_regs The processor register file
 *
 * @return The timer interrupt period in nanoseconds.
 */
extern uint64_t (*xentime_irq)(struct pt_regs *regs);

/*---------------------------------------------------------------------
  -- local variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- private functions
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- public functions
  ---------------------------------------------------------------------*/

