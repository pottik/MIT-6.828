// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/mmu.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display the stack frame", mon_backtrace },
	{ "showMappings", "Display the mapping info.", show_mappings},
	{ "debug", "Enter debug mdoe.", mon_intoDebugMode},
};

// commands_debug used for chanllenge of lab3.
static struct Command commands_debug[] = {
	{ "si", "Step instruction.", debug_stepi},
	{ "quit", "Quit debug mode.", debug_quit},
	{ "c", "Continue.", debug_continue},
};

/***** Implementations of basic kernel monitor commands *****/
void setPermissions(int32_t perm, pte_t *pte);
int32_t s2va(const char* string);
static inline int32_t char2int32(char c);
static inline uint32_t lovelyValidate(uint32_t u);
static int runcmd(char *buf, struct Trapframe *tf, int debug);

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	struct Eipdebuginfo eipInfo;
	uintptr_t ebp = read_ebp(),*t;
	while(ebp != 0){
		t = (uintptr_t*)ebp;
		if(debuginfo_eip((uintptr_t)t[1],&eipInfo) >= 0){
			cprintf("ebp %x eip %x args %08x %08x %08x %08x %08x\n", ebp, t[1],t[2],t[3],t[4],t[5],t[6]);
			cprintf("\t%s:%d: %.*s+%d\n",eipInfo.eip_file,eipInfo.eip_line,eipInfo.eip_fn_namelen,eipInfo.eip_fn_name,eipInfo.eip_fn_addr);
		}
		ebp=t[0];
	}
	return 0;
}

int
show_mappings(int argc, char **argv, struct Trapframe *tf){
	// validate arguments amount
	// argc==3,which means that show pages information between argv[1] and argv[2].
	// argc==4,which means that set all the pages with perm.
	// argc==5,which means that only set page argv[4] with perm.
	// cprintf("debug-> argc :%d\n",argc);
	if(argc < 3){
		cprintf("Invalid arguments.You should pass two virtual address:startVA endVA and optional permissions.\n");
		return 0;
	}

	int32_t start = s2va(argv[1]), end = s2va(argv[2]) + 1, perm = 0, onlyTarget = 0;
	if(start == -1 || end == -1 || end < start){
		cprintf("Invalid virual address was given.\n");
		return 0;
	}

	// obtain optional perm.
	if(argc >= 4){
		char *tmp = argv[3];
		if(tmp[0] == '-')perm = -1;
		else{
			while(*tmp){
				perm = perm * 10 + (*tmp - '0');
				++tmp;
			}
		}
	}
	// obtain onlyTarget while argc==4
	if(argc == 5){
		onlyTarget = s2va(argv[4]);
	}

	uintptr_t u_start_align = ROUNDDOWN((uintptr_t)start, PGSIZE), u_end_align = ROUNDUP((uintptr_t)end, PGSIZE);
	pte_t *pte;
	cprintf("va@start: %08x va@end: %08x\n", u_start_align, u_end_align);
	for(;u_start_align < u_end_align; u_start_align += PGSIZE){
		pte = pgdir_walk(kern_pgdir, (void *)u_start_align, 1);
		if(pte){
			if(argc == 4)setPermissions(perm, pte);
			if(argc == 5 && (uintptr_t)onlyTarget == u_start_align)setPermissions(perm, pte);
			cprintf("va@page: %08x ", u_start_align);
			cprintf("pa@page: %08x @perm:PTE_P %d PTE_U %d PTE_W %d;\n", PTE_ADDR(*pte),lovelyValidate(*pte & PTE_P), lovelyValidate(*pte & PTE_U), lovelyValidate(*pte & PTE_W));
		}
	}

	return 0;
}

int
mon_intoDebugMode(int argc, char **argv, struct Trapframe *tf){
	// This function will only be invoked once.Because next
	char *buf;
	//cprintf("into debug mode."); // debug
	while(1){
		cprintf("=>0x%08x\n", tf->tf_eip);
		buf = readline("(debug)");
		if(buf != NULL){
			if(runcmd(buf, tf, 1) < 0)break;
		}
	}
	return -1;
}

int
debug_stepi(int argc, char** argv, struct Trapframe *tf){
	// This function service for trap.c at the time when the kernel encounter Debug or Breakpoint exceptions.
	// Just a simple stpei function.
	if(tf->tf_trapno == T_BRKPT || tf->tf_trapno == T_DEBUG){
		tf->tf_eflags |= FL_TF; // Set EFLAGS register with Trap flag which is a control flag.
	}else{
		cprintf("You can only use stepi when the processor encounter Debug or Breakpoint exceptions.\n");
	}
	return -1; // must return -1 because we should leave monitor and let user program proceed.
}

int
debug_continue(int argc, char **argv, struct Trapframe *tf){
	if(tf->tf_trapno == T_BRKPT || tf->tf_trapno == T_DEBUG){
		tf->tf_eflags &= ~FL_TF;
	}
	return -1;
}

int
debug_quit(int argc, char **argv, struct Trapframe *tf){
	return -1;
}

/***** Utils for basic kernel monitor commands *****/
void setPermissions(int32_t perm, pte_t *pte){
	if(perm < 0){
		// clear all permissions.
		*pte &= ~0xfff;
	}else{
		*pte |= (perm & 0xfff);
	}
}

int32_t s2va(const char* string){
	// convert a string passed by show_mappings to virtual address.
	int32_t len, va = -1, i, charValue;
	for(i = 0;string[i]; ++i){
		if(i < 2)continue;
		if(i == 2)va = 0;
		if((charValue = char2int32(string[i])) < 0)return -1; // invalid character.
		va = va * 16 + charValue;
	}
	return va;
}

static inline int32_t char2int32(char c){
	// validate a character and convert it to corresponding value.
	if(c >= '0' && c <= '9'){
		return c - '0';
	}else if(c >= 'a' && c <= 'f'){
		return c - 'a' + 10;
	}else if(c >= 'A' && c <= 'F'){
		return 	c - 'A' + 10;
	}
	return -1;
}

static inline uint32_t lovelyValidate(uint32_t u){
	return u > 0?1:0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf, int debug)
{
	int argc;
	char *argv[MAXARGS];
	int i, endi;
	struct Command *theCommands;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	// determine which commands to use.
	if(debug){
		endi = ARRAY_SIZE(commands_debug);
		theCommands = commands_debug;
	}else{
		endi = ARRAY_SIZE(commands);
		theCommands = commands;
	}

	for (i = 0; i < endi; i++) {
		if (strcmp(argv[0], theCommands[i].name) == 0){
			return theCommands[i].func(argc, argv, tf);
		}
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf, 0) < 0)
				break;
	}
}

void
monitor_debug(struct Trapframe *tf){
	char *buf;
	// cprintf("now you got Debug signal."); // debug
	while(1){
		cprintf("=>0x%08x\n", tf->tf_eip);
		buf = readline("(debug)");
		if(buf != NULL){
			if(runcmd(buf, tf, 1) < 0)break;
		}
	}
}

