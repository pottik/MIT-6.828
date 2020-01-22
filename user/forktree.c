// Fork a binary tree of processes and display their structure.

#include <inc/lib.h>

#define DEPTH 3
#define NUM 4 // for challenge-implement fixed-priority scheduler.

void forktree(const char *cur);

void
forkchild(const char *cur, char branch)
{
	char nxt[DEPTH+1];

	if (strlen(cur) >= DEPTH)
		return;

	snprintf(nxt, DEPTH+1, "%s%c", cur, branch);
	if (sfork() == 0) {
		forktree(nxt);
		exit();
	}
}

void
forktree(const char *cur)
{
	cprintf("%04x: I am '%s'\n", sys_getenvid(), cur);

	forkchild(cur, '0');
	forkchild(cur, '1');
}

//void
//umain(int argc, char **argv)
//{
//	forktree("");
//}
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

