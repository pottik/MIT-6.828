#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

void sched_halt(void);

// Choose a user environment to run and run it.
void
sched_yield(void)
{
	uint32_t curenvIndex = (curenv?ENVX(curenv->env_id):0), offset; // curenv might be NULL!
	struct Env *envCandidate = NULL;
	for(offset = 0; offset < NENV; ++offset){
		uint32_t realIndex = (curenvIndex + offset) % NENV;
		if(envs[realIndex].env_status == ENV_RUNNABLE && (!envCandidate || envs[realIndex].env_priority < envCandidate->env_priority)){
			envCandidate = &envs[realIndex];
		}
	}
	// switch to the greatest-priority runnable environment.
	if(curenv && curenv->env_status == ENV_RUNNING && (!envCandidate || curenv->env_priority < envCandidate->env_priority)){
		cprintf("envid@%04x with priority@%d will yield to envid@%04x with priority@%d\n", curenv->env_id, curenv->env_priority, curenv->env_id, curenv->env_priority);
		env_run(curenv);
	}
	if(envCandidate){
		cprintf("envid@%04x with priority@%d will yield to envid@%04x with priority@%d\n", curenv?curenv->env_id:-1, curenv?curenv->env_priority:-1, envCandidate->env_id, envCandidate->env_priority);
		env_run(envCandidate);
	}

	// sched_halt never returns
	sched_halt();
}

// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
	int i;

	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING ||
		     envs[i].env_status == ENV_DYING))
			break;
	}
	if (i == NENV) {
		cprintf("No runnable environments in the system!\n");
		while (1)
			monitor(NULL);
	}

	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(kern_pgdir));

	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);

	// Release the big kernel lock as if we were "leaving" the kernel
	unlock_kernel();

	// Reset stack pointer, enable interrupts and then halt.
	asm volatile (
		"movl $0, %%ebp\n"
		"movl %0, %%esp\n"
		"pushl $0\n"
		"pushl $0\n"
		// Uncomment the following line after completing exercise 13
		"sti\n"
		"1:\n"
		"hlt\n"
		"jmp 1b\n"
	: : "a" (thiscpu->cpu_ts.ts_esp0));
}

