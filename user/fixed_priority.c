// test program for challenge fixed-priority scheduler.

#include<inc/lib.h>

#define NUM 4

void
umain(int argc, char **argv)
{
	unsigned i;
	envid_t envid;
	for(i = 0; i < NUM; ++i){
		if((envid = pfork(i)) > 0){
			// parent environment.
			cprintf("priority@%d envid@%04x yielding.\n", thisenv->env_priority, thisenv->env_id);
			// yield.
			sys_yield();
		}else if(envid == 0){
			// child environment.
			cprintf("env with priority@%d envid@%04x was create.\n", thisenv->env_priority, thisenv->env_id);
		}
	}
}
