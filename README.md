micro-pv
========

Xen Micro Paravirtualized Guest to load a small independent OS

Here are some REALLY IMPORTANT caveats. Things that have made me lose a lot of time:

1.- This is x86_64 only.

2.- YOU MUST USE the compile switch -mno-red-zone. This only seems to become important when you are doing context switching inside the timer hypercall. The mini-os supplied in the Xen source uses cooperative multitasking, so it doesn't seem to have this problem, it's only when you start messing around with interrupt based stack switching that it all falls over in a heap.

3.- YOU MUST USE the -D__XEN_INTERFACE_VERSION__=$(XEN_INTERFACE_VERSION) otherwise the VM doesn't start correctly and again everything falls over in a heap.

4.- Seeing as this is preemptive multitasking, every task MUST HAVE a floating point initialisation. In my application every thread starts in the same initialisation function that calls asm("fninit") and then uses asm("fxsave") in the context switch. As the fxave/fxrstor operations are processor expensive I implement a lazy restore by allowing setting CR0.TS (via the HYPERVISOR_fpu_taskswitch) and then handling the do_device_not_available trap. Make sure that you have a valid context stored via fxsave before calling fxrstor otherwise you'll get a floating point exception. Also put some optimization logic around the fxsave to avoid the overhead of storing every context switch, as most context switches don't need to store the FP context. 
