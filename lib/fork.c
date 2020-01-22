// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	if(!((err & FEC_WR) && (uvpt[PGNUM(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_COW))){
		panic("Permission denied!");
	}
	// if not align,sys_page_alloc will return error code.
	void *addr_align = ROUNDDOWN(addr, PGSIZE);
	// Allocate a page and map it at PFTEMP.
	if((r = sys_page_alloc(0, PFTEMP, (PTE_W | PTE_P | PTE_U))) < 0){
		panic("sys_page_alloc:%e", r);
	}
	// Copy the data from the old page to the new page.
	memcpy(PFTEMP, addr_align, PGSIZE);
	// Then the fault handler map the new page at the appropriate address.
	if((r = sys_page_map(0, (void *)PFTEMP, 0, addr_align, (PTE_W | PTE_P | PTE_U))) < 0){
		panic("sys_page_map:%e", r);
	}
	// Finally,we should unmap the old maps.
	if((r = sys_page_unmap(0, (void *)PFTEMP)) < 0){
		panic("sys_page_unmap:%e", r);
	}
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	uintptr_t pn_va = pn * PGSIZE;
	if((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)){
		if((r = sys_page_map(0, (void *)pn_va, envid, (void *)pn_va, (PTE_COW | PTE_P | PTE_U))) < 0){
			panic("sys_page_map:%e", r);
		}
		if((r = sys_page_map(0, (void *)pn_va, 0, (void *)pn_va, (PTE_COW | PTE_P | PTE_U))) < 0){
			panic("sys_page_map:%e", r);
		}
	}
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	envid_t envid;
	uintptr_t pageVa;
	unsigned pn;
	set_pgfault_handler(pgfault);
	if((envid = sys_exofork()) > 0){
		// parent environment.
		// Copy parent address space.
		for(pageVa = UTEXT; pageVa < USTACKTOP; pageVa += PGSIZE){
			// check permissions.
			pn = PGNUM(pageVa);
			if((uvpd[PDX(pageVa)] & PTE_P) && (uvpt[pn] & PTE_P) && (uvpt[pn] & PTE_U)){
				duppage(envid, pn);
			}
		}
		// Allocate a new page for the child's user exception stack.
		int32_t flag;
		if((flag = sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), (PTE_P | PTE_U | PTE_W))) < 0){
			panic("sys_page_alloc:%e", flag);
		}
		// Copy parent page fault handler setup to the child.
		extern void _pgfault_upcall(void);
		if((flag = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0){
			panic("sys_set_pgfault_upcall:%e", flag);
		}
		// Now we can mark the child environment as runnable.
		if((flag = sys_env_set_status(envid, ENV_RUNNABLE)) < 0){
			panic("sys_env_set_status:%e", flag);
		}
		return envid;
	}else if(envid == 0){
		// Modify "thisenv" in child environment.
		// cprintf("debug sys_getenvid value is:%d\n",ENVX(sys_getenvid()));
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}else{
		// sys_exofork() fail.
		panic("sys_exofork:%e", envid);
	}
}

// Challenge!
int
sfork(void)
{
	envid_t envid;
	uintptr_t pageVa;
	unsigned pn;
	int r;

	set_pgfault_handler(pgfault);
	if((envid = sys_exofork()) > 0){
		// parent environment.
		// Copy parent address space.
		for(pageVa = UTEXT; pageVa < (USTACKTOP - PGSIZE); pageVa += PGSIZE){
			// check permissions.
			pn = PGNUM(pageVa);
			if((uvpd[PDX(pageVa)] & PTE_P) && (uvpt[pn] & PTE_P) && (uvpt[pn] & PTE_U)){
				// Unlike usual fork function,we just need to grant corresponding page permission PTE_W to implement
				// shared memory between parent environment and child environment.
				if((r = sys_page_map(0, (void *)pageVa, envid, (void *)pageVa, (PTE_W | PTE_U | PTE_P))) < 0){
					panic("sys_page_map:%e", r);
				}
				if((r = sys_page_map(0, (void *)pageVa, 0, (void *)pageVa, (PTE_W | PTE_U | PTE_P))) < 0){
					panic("sys_page_map:%e", r);
				}
			}
		}
		// Pages in the stack area should be treated in the usual copy-on-write manner.
		duppage(envid, PGNUM(pageVa));
		// Allocate a new page for the child's user exception stack.
		if((r = sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), (PTE_P | PTE_U | PTE_W))) < 0){
			panic("sys_page_alloc:%e", r);
		}
		// Copy parent page fault handler setup to the child.
		extern void _pgfault_upcall(void);
		if((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0){
			panic("sys_set_pgfault_upcall:%e", r);
		}
		// Now we can mark the child environment as runnable.
		if((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0){
			panic("sys_env_set_status:%e", r);
		}
		return envid;
	}else if(envid == 0){
		// child environment.
		thisenv=&envs[ENVX(sys_getenvid())];
		// cprintf("thisenv@%p\n", thisenv);
		return 0;
	}
	return -E_INVAL;
}
