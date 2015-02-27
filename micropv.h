/* ********************************************************************
   * Project   :
   * Author    : smartin
   ********************************************************************

    Modifications:
    0.01 13/01/2014 Initial version.
*/

#ifndef __MICRO_PV_H__
#define __MICRO_PV_H__

/*---------------------------------------------------------------------
  -- macros (preamble)
  ---------------------------------------------------------------------*/
#define PRINTK(format...) micropv_printk(__FILE__, __LINE__, ##format)
#define PRINTK_BINARY(buffer, buffer_len) micropv_printk_binary(__FILE__, __LINE__, buffer, buffer_len)
#define XBT_NIL ((xenbus_transaction_t)0)
#define SIZEOF_ARRAY(x) (sizeof((x)) / sizeof(*x))

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/
#if !defined(LINUX) && !defined(__KERNEL__)
#include <stdint.h>
#include <stdarg.h>
#include <sys/time.h>
#include <xen/grant_table.h>
#endif

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
  -- forward declarations
  ---------------------------------------------------------------------*/
struct micropv_pci_handle_t;
struct micropv_pci_bus_t;

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/

#if !defined(LINUX) && !defined(__KERNEL__)
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
    unsigned long bp;
    unsigned long bx;
/* arguments: non interrupts/non tracing syscalls only save upto here*/
    unsigned long r11;
    unsigned long r10;
    unsigned long r9;
    unsigned long r8;
    unsigned long ax;
    unsigned long cx;
    unsigned long dx;
    unsigned long si;
    unsigned long di;
    unsigned long orig_ax;
/* end of arguments */
/* cpu exception frame or undefined */
    unsigned long ip;
    unsigned long cs;
    unsigned long flags;
    unsigned long sp;
    unsigned long ss;
/* top of stack page */
};
#else
typedef uint32_t grant_ref_t;
#endif

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

typedef struct micropv_grant_handle_t
{
    uint32_t handle;
    uint64_t dev_bus_addr;
} micropv_grant_handle_t;

typedef struct micropv_pci_device_t
{
    uint32_t domain, bus, slot, fun, vendor, device, rev, class, bar[4];
} micropv_pci_device_t;

/**
 * Can't seem to wrap my brain wround generating a typedef for a
 * function that returns a point to itself, so cast to void *
 *
 * TODO: CORRECT THIS
 */
typedef void * (*micropv_pci_dispatcher_t)(struct micropv_pci_handle_t *);

typedef struct micropv_pci_bus_t
{
    /**
     * Advertised event channel published by backend driver via
     * xenstore
     *
     * @author smartin (7/22/2014)
     */
    int port;
    /**
     * Hypervisor assigned event channel associated to port
     *
     * @author smartin (7/22/2014)
     */
    int channel;
    /**
     * Domain id where the backend is running
     *
     * @author smartin (7/22/2014)
     */
    int32_t backend_domain;
    /**
     * Relative path in our area of the xenstore to PCI device data
     * e.g. device/pci/0
     *
     * @author smartin (7/22/2014)
     */
    char nodename[64];
    /**
     * Absolute path in the xenstore to the PCI device data in the
     * backend domain
     *
     * @author smartin (7/22/2014)
     */
    char backend_path[64];
    /**
     * Page buffer (must be one page in size) that we must share
     * with the backend_domain to talk to the PCI bus
     *
     * @author smartin (7/22/2014)
     */
    char *page_buffer;
    /**
     * Shared memory grant reference for the page_buffer.
     *
     * @author smartin (7/22/2014)
     */
    grant_ref_t grant_ref;
    /**
     * Array of functions that we can call to identify the PCI
     * device. If the device is recognized then the function will
     * return a pointer to a micropv_pci_device_ops_t.
     *
     * @author smartin (7/22/2014)
     */
    micropv_pci_dispatcher_t (**probe)(struct micropv_pci_handle_t *handle, micropv_pci_device_t *device);
    /**
     * Number of probe functions in the probe array
     *
     * @author smartin (7/22/2014)
     */
    int probes;
} micropv_pci_bus_t;

enum micropv_pci_run_status_t
{
    micropv_pci_run_stopped,
    micropv_pci_run_initialisation_backend,
    micropv_pci_run_initialisation_device,
    micropv_pci_run_running,
    micropv_pci_run_stopping,
};

typedef struct micropv_pci_handle_t
{
    /**
     * Current status of the PCI initialisation device
     *
     * @author smartin (7/22/2014)
     */
    enum micropv_pci_run_status_t status;

    /**
     * Definition of the PCI bus
     *
     * @author smartin (7/22/2014)
     */
    micropv_pci_bus_t *bus;

    /**
     * Device data. This is set up to just handle one PCI device.
     * This is enough for me as I only have one, however other
     * people will have to generalize this, either defining this as
     * a fixed array or having the caller assign an array. I think
     * the second option is better.
     *
     * @author smartin (7/23/2014)
     */
    micropv_pci_device_t device;

    /**
     * Function to dispatch PCI device on this PCI bus. This is set
     * up to just handle one PCI device. This is enough for me as I
     * only have one, however other people will have to generalize
     * this, either defining this as a fixed array or having the
     * caller assign an array. I think the second option is better.
     *
     * @author smartin (7/23/2014)
     */
    micropv_pci_dispatcher_t dispatcher;
} micropv_pci_handle_t;

typedef uint32_t xenbus_transaction_t;

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/

// --------- EVENT FUNCTIONS
/**
 * Simulate a processor CLI. Internally this just disables hypervisor events.
 */
void micropv_interrupt_disable(void);

/**
 * Simulate a processor STI. Internally this just enables hypervisor events.
 */
void micropv_interrupt_enable(void);

// --------- SCHEDULER FUNCTIONS
/**
 * Initialise a stack context. This is analogous to thread.
 *
 * @param pt_regs    The processor register file to be initialised.
 * @param start_ptr  Pointer to function that will be run in this context.
 * @param stack_ptr  Pointer to the memory area for the stack for this context.
 * @param stack_size Size of the stack. For x86 platforms the stack grows down so we
 *                   have to initialise the stack pointer to stack_ptr + stack_size.
 */
void micropv_scheduler_initialise_context(struct pt_regs *regs, void *start_ptr, void *stack_ptr, int stack_size);

/**
 * Release the CPU.
 */
void micropv_scheduler_yield(void);

/**
 * Release the CPU and block the domain until the next event.
 */
void micropv_scheduler_block(void);

/**
 * Tell the hypervisor to stop me
 *
 * @author smartin (7/9/2014)
 */
void micropv_exit(void);

// --------- TIME FUNCTIONS
/**
 * Return the current time of day. This is real time, not machine time,
 * and is kept in sync with the HyperVisor.
 *
 * @param tv     Output buffer to receive the current time.
 * @param tz     Timezone, this is currently ignored and only kept for compatibility.
 *
 * @return 0 if successful.
 */
int micropv_time_gettimeofday(struct timeval *tv, void *tz);

/**
 * Nanosecond timer. Guaranteed? to always increment.
 *
 * @return nano seconds since virtual machine was started
 */
uint64_t micropv_time_monotonic_clock(void);

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
void micropv_printk(const char *file, long line, const char *format, ...) __attribute__((format (printf, 3, 4)));

/**
 * Kernel print routine. Whatever is written here is available in the
 * Xen dmesg log for this VM.
 *
 * @param file   The name of the source file from which this function is called.
 *               Use the builtin __FILE__ macro.
 * @param line   The line in the file where this function is called from. Use the
 *               builtin __LINE__ macro.
 * @param format The standard print format to be applied to the following parameters
 * @param args  va_list of arguments to the format
 */
void micropv_printkv(const char *file, long line, const char *format, va_list args);

/**
 * Kernel print routine. Whatever is written here is available in the
 * Xen dmesg log for this VM. This will print binary data in a
 * readable format
 *
 * @param file       The name of the source file from which this function is called.
 *                   Use the builtin __FILE__ macro.
 * @param line       The line in the file where this function is called from. Use the
 *                   builtin __LINE__ macro.
 * @param buffer     The buffer to be printed.
 * @param buffer_len The number of bytes to be printed.
 */
void micropv_printk_binary(const char *file, long line, const char *buffer, size_t buffer_len);

/**
 * Check whether there is anything in the Xen console read buffer that can be read.
 *
 * @return The number of bytes available to be read.
 */
int micropv_console_read_available();

/**
 * Check whether there is room available in the Xen console transmit buffer to send data.
 *
 * @return The number of bytes available to be written.
 */
int micropv_console_write_available();

/**
 * Read the available bytes in the buffer. This DOES NOT block, if there
 * are no bytes to read the the function returns immediately.
 *
 * @param ptr    The output buffer to receive the data.
 * @param len    The length of the buffer to receive the data.
 *
 * @return The number of bytes written to the output buffer.
 */
int micropv_console_read(void *ptr, size_t len);

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
int micropv_console_write(const void *ptr, size_t len);

//--- SHARED MEMORY
/**
 * Publish a shared page. The page MUST be one complete processor page, i.e.
 * declare as char __atribute__((aligned(4096)).
 *
 * @param remote_dom domain id which will be granted access
 * @param name     Name that will appear in the Xen store.
 * @param buffer   Address of the memory to be shared. This must be a pointer to a processor page.
 * @param readonly 0 => read/write access to the shared page, otherwise read only acccess.
 */
void micropv_shared_memory_publish(int remote_dom, const char *name, const void *buffer, int readonly);

/**
 * Consume a shared page. The page MUST be one complete
 * processor page, i.e. declare as char
 * __atribute__((aligned(4096)).
 *
 * @param handle Stores the grant context data.
 * @param name   Name of the entry in the Xen store (relative to
 *               this VM)
 * @param buffer Address of the memory to be mapped. This must
 *               be a pointer to a processor page.
 *
 * @return 0 on success, otherwise -1.
 */
int micropv_shared_memory_consume(micropv_grant_handle_t *handle, const char *name, void *buffer);

/**
 * Release a page that was consumed from a different VM
 *
 * @author smartin (6/3/2014)
 * @param handle Handle created by micropv_shared_memory_consume
 * @param buffer Address of shared memory.
 */
void micropv_shared_memory_unconsume(micropv_grant_handle_t *handle, void *buffer);

void micropv_shared_memory_list();

/**
 * This calls HYPERCALL_update_va_mapping. As far as I can see this can
 * only remap an existing page. Whenever I try to give it an unmapped
 * (above max_pfn) it seems to fall over. However this does fit in quite
 * nicely with our "minimalist" implementation. We don't do dynamic
 * memory! This may change in the future, but for now we just remap
 * existing pages, i.e. the physical address must be in the image.
 *
 * @param physical_address
 *                 Page address in the virtual machine.
 * @param machine_address
 *                 Page address in the Hypervisor memory.
 * @param readonly Page access privilege
 *
 * @return Pointer to the address of the page in the virtual machine or null on failure
 */
void *micropv_remap_page(uint64_t physical_address, uint64_t machine_address, size_t size, int readonly);

/**
 * Convert an address inside the guest machine to an address in the host machine
 *
 * @param virtual_address
 *               Address inside the guest machine
 *
 * @return Corresponding address in the host.
 */
uint64_t micropv_virtual_to_machine_address(uint64_t virtual_address);

/**
 * Convert an address inside the host machine to an address in
 * the guest machine
 *
 * @param machine_address
 *               Address inside the guest machine
 *
 * @return Corresponding address in the guest.
 */
uint64_t micropv_machine_to_virtual_address(uint64_t machine_address);

//--- HYPERVISOR_STATUS

/**
 * Check if the guest is being asked to shutdown
 *
 * @author smartin (7/7/2014)
 *
 * @return int <0 on error, 0 if no shutdown request, >0 if shutdown requested
 */
int micropv_is_shutdown(xenbus_transaction_t xbt);

//--- PCI interface
/**
 * Map a PCI bus into our domain
 *
 * @author smartin (7/9/2014)
 *
 * @param handle stores the device context data
 * @param nodename name of the device in the xenstore
 * @param page_to_share one 4096 byte aligned page that we will share with the pciback domain
 *
 * @return int 0 on success, otherwise -1
 */
int micropv_pci_map_bus(micropv_pci_handle_t *handle);

/**
 * Release a PCI bus from our domain
 *
 * @author smartin (7/9/2014)
 *
 * @param handle handle initialised by micropv_pci_init
 */
void micropv_pci_unmap_bus(micropv_pci_handle_t *handle);

/**
 * Scan the PCI bus to get the pci devices
 *
 * @author smartin (7/18/2014)
 *
 * @param handle stores the device context data
 *
 * @return int 0 on success, otherwise -1
 */
int micropv_pci_scan_bus(micropv_pci_handle_t *handle);

int micropv_pci_conf_read(micropv_pci_handle_t *handle, unsigned int off, unsigned int size, unsigned int *val);
int micropv_pci_conf_write(micropv_pci_handle_t *handle, unsigned int off, unsigned int size, unsigned int val);
int micropv_pci_msi_enable(micropv_pci_handle_t *handle, int (*callback)());
int micropv_pci_msi_disable(micropv_pci_handle_t *handle);

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/
/**
 * This is the register where the overlying operating system must store the
 * pointer to its' timer interrupt handler. I assume that this is where
 * the multitasking context switch is going to occur.
 *
 * @param regs The processor register file
 *
 * @return The timer interrupt period in nanoseconds.
 */
extern uint64_t (*micropv_scheduler_callback)(struct pt_regs *regs);

/**
 * This is the register where the overlying operating system must store the
 * pointer to its floating point context recovery.
 *
 * @param regs The processor register file
 *
 * @return Ignored
 */
extern uint64_t (*micropv_traps_fp_context)(struct pt_regs *regs);

/*---------------------------------------------------------------------
  -- local variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- private functions
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- public functions
  ---------------------------------------------------------------------*/

#endif

