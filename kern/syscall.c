/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.
	user_mem_assert(curenv, (void *) s, len, PTE_P);

	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	env_destroy(e);
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.

	// LAB 4: Your code here.
	struct Env *newEnv, *currentEnv;
	int32_t flag;

	// Obtain current environment corresponding to exerceise 7's hints in lab references.
	// arguments 0 means the current environment.
	envid2env(0, &currentEnv, 1);
	assert(currentEnv);

	if((flag = env_alloc(&newEnv, currentEnv->env_id)) < 0)return flag; // fail on creating a new environment.
	// env_alloc set newEnv's env_status with ENV_RUNNABLE by default,we have to set it with ENV_NOT_RUNNABLE by our own.
	newEnv->env_status = ENV_NOT_RUNNABLE;
	// copy register states from parent environment.
	newEnv->env_tf = currentEnv->env_tf;
	// sys_exofork should return 0 in child environment by set register %eax with 0.Howerver,we set newEnv->env_tf 
	// with ENV_NOT_RUNNABLE above,the new environment can't run util parent environment has allowed it explicitly.
	newEnv->env_tf.tf_regs.reg_eax = 0;
	return newEnv->env_id;
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.

	// LAB 4: Your code here.
	struct Env *theEnv;
	if(envid2env(envid, &theEnv, 1) < 0)return -E_BAD_ENV;
	if(status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE)return -E_INVAL;
	theEnv->env_status = status;
	return 0;
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3), interrupts enabled, and IOPL of 0.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 5: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
	struct Env *theEnv;
	if(envid2env(envid, &theEnv, 1) < 0)return -E_BAD_ENV;
	// cprintf("this tf va@0x%p\n", tf);
	user_mem_assert(theEnv, tf, sizeof(struct Trapframe), PTE_U);
	// Clear IOPL bits.
	tf->tf_eflags &= ~FL_IOPL_MASK;
	// Interrupts enabled.
	tf->tf_eflags |= FL_IF;
	// code protection level 3(CPL 3).
	tf->tf_cs |= GD_UT | 3;
	// set envid's trap frame to tf.
	theEnv->env_tf = *tf;
	return 0;
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
	struct Env *thisEnv;
	if(envid2env(envid, &thisEnv, 1) < 0)return -E_BAD_ENV;
	thisEnv->env_pgfault_upcall = func;
	return 0;
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 4: Your code here.
	struct Env *theEnv;
	struct PageInfo *pageInfo;
	int32_t leastPerm = (PTE_U | PTE_P);
	if(envid2env(envid, &theEnv, 1) < 0)return -E_BAD_ENV;
	if((uintptr_t)va >= UTOP || ((uintptr_t)va & (PGSIZE - 1)))return -E_INVAL;
	if((perm & leastPerm) != leastPerm || (perm & (~PTE_SYSCALL)))return -E_INVAL; // PTE_SYSCALL == PTE_U | PTE_P | PTE_AVAIL | PTE_W
	if(!(pageInfo = page_alloc(ALLOC_ZERO)))return -E_NO_MEM;
	if(page_insert(theEnv->env_pgdir, pageInfo, va, perm) < 0){
		page_free(pageInfo);
		return -E_NO_MEM;
	}
	return 0;
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 4: Your code here.
	pte_t *srcPte;
	int32_t leastPerm = (PTE_U | PTE_P);
	struct PageInfo *srcPageInfo;
	struct Env *srcEnv, *dstEnv;
	if(envid2env(srcenvid, &srcEnv, 1) < 0 || envid2env(dstenvid, &dstEnv, 1) < 0)return -E_BAD_ENV;
	if((uintptr_t)srcva >= UTOP || (uintptr_t)dstva >= UTOP || ((uintptr_t)srcva & (PGSIZE - 1)) || ((uintptr_t)dstva & (PGSIZE - 1)))return -E_INVAL;
	if((perm & leastPerm) != leastPerm || (perm & (~PTE_SYSCALL)))return -E_INVAL; // PTE_SYSCALL == PTE_U | PTE_P | PTE_AVAIL | PTE_W
	if(!(srcPageInfo = page_lookup(srcEnv->env_pgdir, srcva, &srcPte)))return -E_INVAL;
	if((perm & PTE_W) && !((*srcPte) & PTE_W))return -E_INVAL;
	return page_insert(dstEnv->env_pgdir, srcPageInfo, dstva, perm);
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().

	// LAB 4: Your code here.
	struct Env *theEnv;
	if(envid2env(envid, &theEnv, 1) < 0)return -E_BAD_ENV;
	if((uintptr_t)va >= UTOP || (uintptr_t)va & (PGSIZE - 1))return -E_INVAL;
	page_remove(theEnv->env_pgdir, va);
	return 0;
}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	struct Env *receiverEnv, *currentEnv;
	int32_t error, leastPerm = (PTE_U | PTE_P);
	// obtain current environment.
	envid2env(0, &currentEnv, 0);
	assert(currentEnv);
	// Environment envid doesn't currently exist.
	if((error = envid2env(envid, &receiverEnv, 0)) < 0)return error;
	// Envid is not currently blocked in sys_ipc_recv or another enviironment managed to send first.
	if(!receiverEnv->env_ipc_recving)return -E_IPC_NOT_RECV;
	// Need to send page currently mapped at 'srcva'.
	if((uintptr_t)srcva < UTOP){
		struct PageInfo *pageInfo;
		pte_t *pte;
		// srcva is not page-aligned.
		if(((uintptr_t)srcva & (PGSIZE - 1)))return -E_INVAL;
		// Permissions deny.
		if((int32_t)(leastPerm & perm) != leastPerm || (perm & (~PTE_SYSCALL)))return -E_INVAL;
		// srcva is not mapped in the caller's address space.
		if(!(pageInfo = page_lookup(currentEnv->env_pgdir, srcva, &pte)))return -E_INVAL;
		// if(perm & PTE_W),but srcva is read-only in the current environment's address space.
		if((perm & PTE_W) && !((*pte) & PTE_W))return -E_INVAL;
		if((uintptr_t)receiverEnv->env_ipc_dstva < UTOP){
			if((error = page_insert(receiverEnv->env_pgdir, pageInfo, receiverEnv->env_ipc_dstva, perm)) < 0)return error;
		}
	}
	// restore status.
	receiverEnv->env_ipc_recving = 0;
	receiverEnv->env_ipc_value = value;
	receiverEnv->env_ipc_from = currentEnv->env_id;
	receiverEnv->env_ipc_perm = perm;
	// Then we should mark the receiver environment as ENV_RUNNABLE.
	receiverEnv->env_status = ENV_RUNNABLE;
	// Return 0 from the paused sys_env_recv system call.
	// Howerver,the corresponding sys_ipc_recv function will never return,so we need to modify receiver's environment's trapframe.
	receiverEnv->env_tf.tf_regs.reg_eax = 0;
	// sys_ipc_try_send could return 0.
	return 0;
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
	if((uintptr_t)dstva < UTOP && (uintptr_t)dstva & (PGSIZE - 1))return -E_INVAL;
	// Willing wo receive a page of data.
	struct Env *currentEnv;
	envid2env(0, &currentEnv, 0);
	assert(currentEnv);

	currentEnv->env_status = ENV_NOT_RUNNABLE;
	currentEnv->env_ipc_recving = 1;
	currentEnv->env_ipc_dstva = dstva;
	currentEnv->env_ipc_from = 0;
	sys_yield();
	return 0;
}

// allocate a new page as this environmnt's page directory and map
// it to va temporarily.
static int
sys_exec_alloc_pgdir(void *va)
{
	struct Env *currentEnv;
	struct PageInfo *pageInfo = NULL;
	pde_t *new_pgdir;

	envid2env(0, &currentEnv, 1);
	assert(currentEnv);
	// step 1) we should check that whether the page correspondingly to pgdir_temp
	//         is present or not.At the same time,we have to ensure that the virtual
	//         address va is page-aligned.
	if(((uintptr_t)va & (PGSIZE - 1)) || page_lookup(currentEnv->env_pgdir, va, NULL)){
		cprintf("[DEBUG]sys_exec_alloc_pgdir:va is now mapped or va was not page aligned.\n");
		return -E_INVAL;
	}
	// step 2) allocate a new page as this environment's page directory.
	if(!(pageInfo = page_alloc(ALLOC_ZERO))){
		return -E_NO_MEM;
	}
	// step 3) map the new page to va temporarily.
	if(page_insert(currentEnv->env_pgdir, pageInfo, va, (PTE_W | PTE_P | PTE_U)) < 0){
		return -E_NO_MEM;
	}
	// step 4) copy this environment's old pgdir here just like what we have done before.
	//         Don't forget to increment its ref counter.
	//         Most importantly,it's necessary switch page directory for that we are now
	//         in kernel mode.
	//cprintf("[INFO]Before cr3 is %p.\n", rcr3());
	lcr3(PADDR(currentEnv->env_pgdir));
	//cprintf("[INFO]After cr3 is %p.\n", rcr3());
	memcpy(va, kern_pgdir, PGSIZE);
	++pageInfo->pp_ref;
	//lcr3(PADDR(kern_pgdir));
	//cprintf("[INFO]Finally cr3 is %p.\n", rcr3());
	return 0;
}

// lab 5's challenge - implement Unix-Like exec.
static int
sys_exec_map(pde_t *pg_dir, void *srcva, void *dstva, int perm)
{
	struct PageInfo *pageInfo = NULL;
	struct Env *currentEnv;
	int32_t leastPerm = (PTE_U | PTE_P);
	envid2env(0, &currentEnv, 1);
	assert(currentEnv);

	// step 0)
	if((uintptr_t)srcva >= UTOP || (uintptr_t)dstva >= UTOP || ((uintptr_t)srcva & (PGSIZE - 1)) || ((uintptr_t)dstva & (PGSIZE - 1))){
		cprintf("[DEBUG]sys_exec_map:step 0 failed.\n");
		return -E_INVAL;
	}

	// step 1) check pg_dir present.
	if(!page_lookup(currentEnv->env_pgdir, pg_dir, NULL)){
		cprintf("[DEBUG]sys_exec_map:pg_dir not present.\n");
		return -E_INVAL;
	}
	// step 2) check source page present.
	if(!( pageInfo = page_lookup(currentEnv->env_pgdir, srcva, NULL))){
		cprintf("[DEBUG]sys_exec_map:step 2 failed.\n");
		return -E_INVAL;
	}
	// step 3)
	if((perm & leastPerm) != leastPerm || (perm & (~PTE_SYSCALL))){
		cprintf("[DEBUG]sys_exec_map:step 3 failed,permisson denied.\n");
		return -E_INVAL;
	}
	// step 4) map pageInfo to dstva.
	// cprintf("[DEBUG]pg_dir is %p and dstva is %p\n", pg_dir, dstva);
	return page_insert(pg_dir, pageInfo, dstva, perm);
}

// lab 5's challenge - implement Unix-Like exec.
static int
sys_exec_replace_pgdir(pde_t *pg_dir, uintptr_t esp, uintptr_t e_entry)
{
	struct PageInfo *pageInfo = NULL;
	struct Env *currentEnv = NULL;
	pde_t *old_pgdir;
	pte_t *pt;
	uint32_t pdeno, pteno;
	physaddr_t pa;

	envid2env(0, &currentEnv, 1);
	assert(currentEnv);
	if(!(pageInfo = page_lookup(currentEnv->env_pgdir, (void *)pg_dir, NULL))){
		cprintf("[DEBUG]sys_exec_replace_pgdir:page_lookup:pageInfo is null.\n");
		return -E_INVAL;
	}
	// save the old page directory.
	old_pgdir = currentEnv->env_pgdir;
	currentEnv->env_pgdir = (pde_t *)page2kva(pageInfo);
	// UVPT maps the env's own page table read-only.
	currentEnv->env_pgdir[PDX(UVPT)] = PADDR(currentEnv->env_pgdir) | PTE_P | PTE_U;

	// we have replaced page directory.However,we didn't free old page directory and page table,
	// that's what we are going to do next.
	for(pdeno = 0; pdeno < PDX(UTOP); ++pdeno){
		if(!(old_pgdir[pdeno] & PTE_P))
			continue;
		// find the pa and va of the page table.
		pa = PTE_ADDR(old_pgdir[pdeno]);
		pt = (pte_t *)KADDR(pa);
		// unmap all PTEs in this page table.
		for(pteno = 0; pteno <= PTX(~0); ++pteno){
			if(pt[pteno] & PTE_P)
				page_remove(old_pgdir, PGADDR(pdeno, pteno, 0));
		}
		// free the page table itself.
		old_pgdir[pdeno] = 0;
		page_decref(pa2page(pa));
	}
	// free the page directory.
	pa = PADDR(old_pgdir);
	old_pgdir = 0;
	page_decref(pa2page(pa));

	// modify some status of this environment.
	currentEnv->env_tf.tf_esp = esp;
	currentEnv->env_tf.tf_eip = e_entry;
	env_run(currentEnv);
	return 0; // never get here,just eliminate compiling error.
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.

	switch (syscallno) {
		case SYS_cputs:sys_cputs((char *)a1, (size_t)a2);return 0;
		case SYS_cgetc:return sys_cgetc();
		case SYS_getenvid:return (envid_t)sys_getenvid();
		case SYS_env_destroy:return sys_env_destroy((envid_t)a1);
		case SYS_yield:sys_yield();return 0;
		case SYS_exofork:return sys_exofork();
		case SYS_env_set_status:return sys_env_set_status((envid_t)a1, a2);
		case SYS_page_alloc:return sys_page_alloc((envid_t)a1, (void *)a2, a3);
		case SYS_page_map:return sys_page_map((envid_t)a1, (void *)a2, (envid_t)a3, (void *)a4, a5);
		case SYS_page_unmap:return sys_page_unmap((envid_t)a1, (void *)a2);
		case SYS_env_set_pgfault_upcall:return sys_env_set_pgfault_upcall((envid_t)a1, (void *)a2);
		case SYS_ipc_try_send:return sys_ipc_try_send((envid_t)a1, (uint32_t)a2, (void *)a3, (unsigned)a4);
		case SYS_ipc_recv:return sys_ipc_recv((void *)a1);
		case SYS_env_set_trapframe:return sys_env_set_trapframe((envid_t)a1, (struct Trapframe *)a2);
		case SYS_exec_alloc_pgdir:return sys_exec_alloc_pgdir((void *)a1);
		case SYS_exec_map:return sys_exec_map((pde_t *)a1, (void *)a2, (void *)a3, a4);
		case SYS_exec_replace_pgdir:return sys_exec_replace_pgdir((pde_t *)a1, (uintptr_t)a2, (uintptr_t)a3);
		default:
			return -E_INVAL;
	}
}

