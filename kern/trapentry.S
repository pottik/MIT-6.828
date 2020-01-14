/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>



###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

/*
 * My own macro for exercise 4's chanllenge of lab3.
 */
#define TRAPHANDLER_MINE(name, num)                                     \
.data;                                                                  \
	.long name;             /* entry point's symbol */              \
.text;                                                                  \
	.global name;		/* define global symbol for 'name' */   \
	.type name, @function;  /* symbol type is function */           \
	.align 2;               /* align function definition */         \
name:                           /* function starts here */              \
	pushl $(num);                                                   \
	jmp _alltraps

#define TRAPHANDLER_NOEC_MINE(name, num)                                \
.data;                                                                  \
	.long name;                                                     \
.text;                                                                  \
	.global name;		/* define global symbol for 'name' */   \
	.type name, @function;  /* symbol type is function */           \
	.align 2;               /* align function definition */         \
name:                           /* function starts here */              \
	pushl $0;                                                       \
	pushl $(num);                                                   \
	jmp _alltraps

.data
	.global entryPointOfTraps
	entryPointOfTraps:
.text
// Exceptions without error code.
TRAPHANDLER_NOEC_MINE(divideErrorHandler, T_DIVIDE);
TRAPHANDLER_NOEC_MINE(debugHandler, T_DEBUG);
TRAPHANDLER_NOEC_MINE(NMIHandler, T_NMI);
TRAPHANDLER_NOEC_MINE(breakpointHandler, T_BRKPT);
TRAPHANDLER_NOEC_MINE(overflowHandler, T_OFLOW);
TRAPHANDLER_NOEC_MINE(BOUNDRangeExceededHandler, T_BOUND);
TRAPHANDLER_NOEC_MINE(invalidOpcodeHandler, T_ILLOP);
TRAPHANDLER_NOEC_MINE(deviceNotAvailableHandler, T_DEVICE);
TRAPHANDLER_MINE(doubleFaultHandler, T_DBLFLT);
TRAPHANDLER_NOEC_MINE(coprocessorSegmentOverrunHandler, T_COPROC);// Just padding.
TRAPHANDLER_MINE(invalidTSSHandler, T_TSS);
TRAPHANDLER_MINE(segmentNotPresentHandler, T_SEGNP);
TRAPHANDLER_MINE(stackFaultHandler, T_STACK);
TRAPHANDLER_MINE(generalProtectionHandler, T_GPFLT);
TRAPHANDLER_MINE(pageFaultHandler, T_PGFLT);
TRAPHANDLER_MINE(reserved, T_RES); // Just padding.
TRAPHANDLER_NOEC_MINE(floatingPointErrorHandler, T_FPERR);
TRAPHANDLER_NOEC_MINE(alignmentCheckHandler, T_ALIGN);
TRAPHANDLER_NOEC_MINE(machineCheckHandler, T_MCHK);
TRAPHANDLER_NOEC_MINE(SIMDFloatingPointExceptionHandler, T_SIMDERR);
TRAPHANDLER_NOEC_MINE(syscallHandler, T_SYSCALL);


/*
 * Origin code passing the test case for lab3:exercise 4
 */

/*
 * generating entry points for the different traps.
 *
// Exceptions without error code.
TRAPHANDLER_NOEC(divideErrorHandler, T_DIVIDE);
TRAPHANDLER_NOEC(debugHandler, T_DEBUG);
TRAPHANDLER_NOEC(NMIHandler, T_NMI);
TRAPHANDLER_NOEC(breakpointHandler, T_BRKPT);
TRAPHANDLER_NOEC(overflowHandler, T_OFLOW);
TRAPHANDLER_NOEC(BOUNDRangeExceededHandler, T_BOUND);
TRAPHANDLER_NOEC(invalidOpcodeHandler, T_ILLOP);
TRAPHANDLER_NOEC(deviceNotAvailableHandler, T_DEVICE);


// Exceptions with error code.You can find this information in chapter 9.10 of 80386 programmer's references mannual.
TRAPHANDLER(doubleFaultHandler, T_DBLFLT);
// TRAPHANDLER_NOEC(coprocessorSegmentOverrunHandler, T_COPROC); reserved,just ignore.
TRAPHANDLER(invalidTSSHandler, T_TSS);
TRAPHANDLER(segmentNotPresentHandler, T_SEGNP);
TRAPHANDLER(stackFaultHandler, T_STACK);
TRAPHANDLER(generalProtectionHandler, T_GPFLT);
TRAPHANDLER(pageFaultHandler, T_PGFLT);

// Exceptions without error code.
TRAPHANDLER_NOEC(floatingPointErrorHandler, T_FPERR);
TRAPHANDLER_NOEC(alignmentCheckHandler, T_ALIGN);
TRAPHANDLER_NOEC(machineCheckHandler, T_MCHK);
TRAPHANDLER_NOEC(SIMDFloatingPointExceptionHandler, T_SIMDERR);
 *
 */

/*
 * Lab 3: Your code here for _alltraps
 * Key point from lab reference:
 * 	1. push values to make the stack look like a struct Trapframe;
 * 	2. load GD_KD into %ds and %es;
 * 	3. pushl %esp to pass a pointer to the Trapframe as an argument to trap();
 * 	4. call trap (can trap ever return?);
 * According to inc/trap.h and the macro TRAPHANDLER,we can know that we should push %ds and %es register after
 * the tf_trapno because the tf_trapno was pushed onto stack by x86 hardware.
 */
.global _alltraps
_alltraps:
	pushl %ds
	pushl %es
	pushal
	movw $GD_KD, %ax
	movw %ax, %ds
	movw %ax, %es
	pushl %esp
	call trap

