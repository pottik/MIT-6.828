// hello, world
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	int r;
	cprintf("hello, world!\n");
	cprintf("[BEFORE]parent environment %08x\n", thisenv->env_id);
	if((r = fork()) < 0){
		cprintf("[DEBUG]fork failed for %e.\n", r);
		return;
	}
	// printed by parent and child environment.
	cprintf("[COMMON]environment %08x\n", thisenv->env_id);
	if(r > 0){
		// parent environment.
		cprintf("[PARENT]hello\n");
	}else if(r == 0){
		// child environment.
		cprintf("[CHILD]world!\n");
		if((r = execl("testshell", "testshell", 0)) < 0){
			panic("[DEBUG]panic for %e", r);
		}
	}
	cprintf("[PARENT]Oh!\n");
}
