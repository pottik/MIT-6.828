#include <inc/mmu.h>
#include <inc/memlayout.h>

// Page fault upcall entrypoint.

// This is where we ask the kernel to redirect us to whenever we cause
// a page fault in user space (see the call to sys_set_pgfault_handler
// in pgfault.c).
//
// When a page fault actually occurs, the kernel switches our ESP to
// point to the user exception stack if we're not already on the user
// exception stack, and then it pushes a UTrapframe onto our user
// exception stack:
//
//	trap-time esp
//	trap-time eflags
//	trap-time eip
//	utf_regs.reg_eax
//	...
//	utf_regs.reg_esi
//	utf_regs.reg_edi
//	utf_err (error code)
//	utf_fault_va            <-- %esp
//
// If this is a recursive fault, the kernel will reserve for us a
// blank word above the trap-time esp for scratch work when we unwind
// the recursive call.
//
// We then have call up to the appropriate page fault handler in C
// code, pointed to by the global variable '_pgfault_handler'.

.text
.globl _pgfault_upcall
_pgfault_upcall:
	// Call the C page fault handler.
	pushl %esp			// function argument: pointer to UTF
	movl _pgfault_handler, %eax
	call *%eax
	addl $4, %esp			// pop function argument
	
	// Now the C page fault handler has returned and you must return
	// to the trap time state.
	// Push trap-time %eip onto the trap-time stack.

	movl 0x28(%esp), %eax   // UTrapFrame.utf_eip
	subl $4, 0x30(%esp)     // UTrapFrame.utf_esp - 4
	movl 0x30(%esp), %edx   // Set register %edx to UTrapFrame.utf_esp - 4
	movl %eax, (%edx)

	addl $0x8, %esp         // Set %esp to the end of struct PushRegs,see more details at inc/trap.h
	popal                   // Restore the trap-time registers.

	addl $0x4, %esp         // now we are located at trap-time eip,so we have to increment %esp with 4.
	popfl                   // Restore eflags register.

	popl %esp               // Just restore the adjusted trap-time stack pointer.

	ret                     // We are now on the trap-time stack,since we have saved trap-time eip above
				// trap-time esp,ret instruction will pop this trap-time eip to register %eip
				// which known as PC at this time.Thus,we can return to re-execute the instruction
				// that faulted.
