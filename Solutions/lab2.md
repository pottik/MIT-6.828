# <span>Contents</span>
* [Part 1:Physical Page Management](#OSELAB2_1)
	* [Exercise 1](#OSELAB2_1.1)
		* [Key Point](#OSELAB2_1.1.1)
* [Part 2:Virtual Memory](#OSELAB2_2)
	* [Exercise 2,3](#OSELAB2_2.2)
		* [Key Point](#OSELAB2_2.2.1)
	* [Exercise 4](#OSELAB2_2.4)
* [Part 3:Kernel Address Space](#OSELAB2_3)
	* [Exercise 5](#OSELAB2_3.5)
		* [Key Point](#OSELAB2_3.5.1)
* [Questions](#OSELAB2_4)
* [Challenge](#OSELAB2_Challenge)
* [Code implemention for all exercises](#OSELAB2_CODE)
* [REFERENCES](#OSELAB2_ref)

<!-- more -->

---

# <span id="OSELAB2_1">Part 1:Physical Page Management</span>

---

## <span id="OSELAB2_1.1">Exercise 1</span>

### <span id="OSELAB2_1.1.1">Key point</span>
后续内容建议配合该图食用： <div align="center">![图1-1 简单示意图](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab2/exercise1/%E7%89%A9%E7%90%86%E5%86%85%E5%AD%98%E7%A4%BA%E6%84%8F%E5%9B%BE.PNG)</div>
在LAB1做完之后，物理内存当中就已经被加载了内核代码以及数据了（.text Section,.data Section)。kern/init.c当中下一步就是调用`mem_init()`，通过LAB1的学习以及kern/pmap.c当中的`mem_init`以及`boot_alloc`函数的注释，能够了解到boot_alloc是在尚未划分物理页面时在kernel的.bss segment之后分配一个页面大小的内存空间作为kernel page directory,并且分配一定大小的内存空间作为Page Info arrays（该内存空间的大小取决于npages以及sizeof(struct PageInfo),并且还要按PGSIZE对齐）。因此在`mem_init`中，首先初始化了kernerl page directory，随后给Page Info arrays分配了内存空间，用来存储整个物理内存空间对应的页面信息(Page Information)，在exercise 1当中，最后就是调用`page_init`来初始化整个物理内存空间对应的页面信息，理解了上图，写出`page_init`以及整个exercise 1就不难了。  

但是仍然有几个注意点：
1. 记录一个页面信息时，将PageInfo中的pp_ref置为1并不是表明其in use或者could not allocate，实际上正确的方式就是忽略这个页面（但是由于C语言需要初始化，因此将pp_ref置为0，pp_link置为NULL即可），虽然将pp_ref置为1好像也不会出错，但是不建议这么做。
2. npages_basemem实际上低于1MB并且低于其中IO hole(在LAB1当中有解释！）的部分的物理页面的数量，而npages则是整个物理内存空间包含的物理页面的数量。
3. **关于上图如何验证，事实上make qemu启动后在i386_detect_memory函数中会输出base=640K，然后你还会发现在该函数中有npages_basemem = basemem / (PGSIZE / 1024);事实上这就完美验证了！**

**代码实现见[Code implemention for all exercises](#OSELAB2_CODE)**.

---

# <span id="OSELAB2_2">Part 2:Virtual Memory</span>

---

## <span id="OSELAB2_2.2">Exercise 2,3</span>
### <span id="OSELAB2_2.2.1">Key point</span>
这一部分就是让我们认真阅读80386 programmer mannual.这部分基础知识就不再赘述。但是值得一提的是，在80386中，并不是没有segment translation的机制，只是在boot.S当中加载了全局描述符表，该GDT将所有的段寄存器的Base address都设置为0，段寄存器寻址上限设置为0xffff ffff，因此虚拟地址经过Segment translation之后与线性地址等价，对外表现为没有segment translation的机制，但是不能够说80386将segment translation机制禁用了。下面这个简单的示意图还是很重要的：
```

           Selector  +--------------+         +-----------+
          ---------->|              |         |           |
                     | Segmentation |         |  Paging   |
Software             |              |-------->|           |---------->  RAM
            Offset   |  Mechanism   |         | Mechanism |
          ---------->|              |         |           |
                     +--------------+         +-----------+
            Virtual                   Linear                Physical
```
此外，就是exercise3提到的进入qemu monitor的快捷键在我这不顶用，根据这个老哥的博客[fatsheep-mit6.828-lab2](https://www.cnblogs.com/fatsheep9146/p/5124921.html),这个是可行的。
```
qemu-system-i386 -hda obj/kern/kernel.img -monitor stdio -gdb tcp::26000 -D qemu.log
```

## <span id="OSELAB2_2.4">Exercise 4</span>
无特殊注意事项，**代码实现见[Code implemention for all exercises](#OSELAB2_CODE)**.

---

# <span id="OSELAB2_3">Part 3:Kernel Address Space</span>

---

## <span id="OSELAB2_3.5">Exercise 5</span>

### <span id="OSELAB2_3.5.1">Key point</span>
完善`mem_init()`的虚拟内存映射代码即可。
```c
boot_map_region(kern_pgdir, UPAGES, PTSIZE, PADDR(pages), (PTE_U | PTE_P));
```
将pages那部分内容映射到虚拟地址`UPAGES`处，这里的`PTSIZE`实际上与`ROUNDUP(sizeof(struct PageInfo)*npages, PGSIZE)`是等价的，虽然它们值不相等，但是都能通过校验函数。用PTSIZE可能是源于inc/memlayout.h中的示意图，使用另外一种是因为这是PageInfo实际分配的物理存储空间大小。
```c
boot_map_region(kern_pgdir, KSTACKTOP-KSTKSIZE, KSTKSIZE, PADDR(bootstack), (PTE_W | PTE_P));
```
将内核栈映射到虚拟地址KSTACKTOP-KSTKSIZE处。
```c
boot_map_region(kern_pgdir, KERNBASE, -KERNBASE, 0, (PTE_W | PTE_P));
```
将整个物理存储空间映射到KERNBASE之上，在32位机器上将-KERNBASE按照无符号数解释恰好就是2^32-KERNBASE。


**代码实现见[Code implemention for all exercises](#OSELAB2_CODE)**.

---

# <span id="OSELAB2_4">Questions</span>

---
>1.Assuming that the following JOS kernel code is correct,what type should variable x have,`uintptr_t` or `physaddr_t`? 

```c
mystery_t x;
char* value = return_a_pointer();
*value = 10;
x = (mystery_t) value;
```
很显然，返回的是指针，必然是虚拟地址，因此x的类型是`uintptr_t`.


>2.What entries (rows) in the page directory have been filled in at this point? What addresses do they map and where do they point? In other words, fill out this table as much as possible: <div align="center">![图4-1 答案仅供参考](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab2/question/question2.PNG)</div>



>3.We have placed the kernel and user environment in the same address space. Why will user programs not be able to read or write the kernel's memory? What specific mechanisms protect the kernel memory?

无论是Page directory entry还是Page table entry，其低12位的控制bit起到了访问控制（读/写）以及校验优先等级（privileged level）的作用。通过设置相应的bit位就能够控制user对page的访问权限。具体参阅[Intel 80386 Reference Programmer's Manual](https://pdos.csail.mit.edu/6.828/2018/readings/i386/toc.htm)


>4.What is the maximum amount of physical memory that this operating system can support? Why?

JOS使用32位系统，并且通过mem_init的设置虚拟内存对物理内存的映射的代码，我们将物理内存从0开始映射到了KERNBASE之上，而KERNBASE是0xF000 0000，32位系统最大虚拟地址为0xFFFF FFFF，因此无论实际物理内存有多大，JOS最大也只能映射2GB的物理内存。从另外一个角度来看，JOS为Page Info分配了4MB的空间，每个`struct PageIngo`需要8字节空间，因此最多有4MB/8=512K个PageInfo对应于512K个Page，每个Page 4KB，因此最大物理内存空间为2GB。


>5.How much space overhead is there for managing memory, if we actually had the maximum amount of physical memory? How is this overhead broken down?

倘若JOS映射了2GB的物理内存，那么就有2GB/4KB=512K个物理页面，每个物理页面对应于4 bytes的page table entry，因此这部分花销是512K*4=2MB。除此之外，还有4KB的Page directory以及4MB的Page Info，因此总的花销是2MB + 4MB + 4KB.

>6.Revisit the page table setup in kern/entry.S and kern/entrypgdir.c. Immediately after we turn on paging, EIP is still a low number (a little over 1MB). At what point do we transition to running at an EIP above KERNBASE? What makes it possible for us to continue executing at a low EIP between when we enable paging and when we begin running at an EIP above KERNBASE? Why is this transition necessary?

在LAB1当中已经提到了，在`jmp *%eax`之后，EIP从高于KERNBASE的地址处开始执行。之所以这样可行是因为entrypgdir.c将虚拟地址[0, 4MB)还有[KERNBASE, KERNBASE+4MB)映射到了物理地址[0, 4MB)。之所以要将[0, 4MB)的虚拟地址空间也映射到物理地址[0, 4MB)是因为在开启Paging之后，entry.S中还有少许指令需要执行，当其执行完之后，就会进入C语言编写的代码来对内核进行初始化，而C实际上只支持虚拟内存，因此这种转移是必要的。

---

# <span id="OSELAB2_Challenge">Challenges</span>

---
>Challenge! We consumed many physical pages to hold the page tables for the KERNBASE mapping. Do a more space-efficient job using the PTE_PS ("Page Size") bit in the page directory entries. This bit was not supported in the original 80386, but is supported on more recent x86 processors.

查阅[Intel® 64 and IA-32 Architectures Software Developer’s Manual](https://pdos.csail.mit.edu/6.828/2018/readings/ia32/IA32-3A.pdf)。例如基于x86架构的IA-32处理器以及 Pentium II处理器都支持了在Page directory entry中PTE_PS位的设置。在**同时设置了控制寄存器CR4的PSE flag以及Page direcotry entry中的PTE_PS flag之后，该Page directory entry实际上就存储了指向4MB物理页的指针（即22~31bit位）**这时候实际上二级页表不会起作用。但值得注意的是，按照Intel参考文档的说明，若没有设置CR4的PSE flag，即便设置了PTE_PS flag物理页也只能是4KB，不会是4MB。
>【Reference from volume 3-27】When the PSE flag in CR4 is set, both 4-MByte pages and page tables for 4-KByte
pages can be accessed from the same page directory. If the PSE flag is clear, only
page tables for 4-KByte pages can be accessed (regardless of the setting of the PS
flag in a page-directory entry).

我们熟知的4KB页面大小的Page Directory Entry： <div align="center">![图5-1 4KB页面大小的Page Directory Entry](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab2/challenge/%E6%99%AE%E9%80%9APDE.PNG)</div>
当设置了PSE flag以及PTE_PS flag之后，页面大小为4MB，这时的Page Directory Entry: <div align="center">![图5-2 4MB页面大小的Page Directory Entry](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab2/challenge/%E5%BC%80%E5%90%AFPTE_PSflag%E7%9A%844MBPDE.PNG)</div>
相应的linear address translation也与之前两级页表有所不同： <div align="center">![图5-3 4MB页面大小对应的地址翻译示意图](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab2/challenge/4MB%E9%A1%B5%E9%9D%A2%E5%9C%B0%E5%9D%80translation.PNG)</div>
当页面大小是4MB时，二级页表就不起作用了，因此，不仅能够减少存储页面信息的开销，还能减少页表的开销。

---

>Challenge! Extend the JOS kernel monitor with commands to:

实现了一个简单版本，代码比较丑陋，没有做到能对任何的参数都不会产生bug，仅限简单的参数校验。下方是核心`showMappings`函数，其它util工具函数参见github代码。
```c
int show_mappings(int argc, char **argv, struct Trapframe *tf){
	// validate arguments amount
	// argc==3,which means that show pages information between argv[1] and argv[2].
	// argc==4,which means that set all the pages with perm.
	// argc==5,which means that only set page argv[4] with perm.
	cprintf("debug-> argc :%d\n",argc);
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
```
效果示意图： <div align="center">![图5-4 扩展指令效果示意图（仅供参考）](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab2/challenge/monitorExtended%E7%A4%BA%E6%84%8F%E5%9B%BE.PNG)</div>

---

# <span id="OSELAB2_CODE">Code implemention for all exercises</span>
**See github:**[YanTang Qin- MIT6.828](https://github.com/QinStaunch/MIT-6.828)

---

# <center><span id="OSELAB2_ref">REFERENCES</span></center>

【1】 [Intel 80386 Reference Programmer's Manual](https://pdos.csail.mit.edu/6.828/2018/readings/i386/toc.htm)  
【2】 [Intel® 64 and IA-32 Architectures Software Developer’s Manual](https://pdos.csail.mit.edu/6.828/2018/readings/ia32/IA32-3A.pdf)  
【3】 [XV6 - a simple, Unix-like teaching operating system - Reference Book](https://pdos.csail.mit.edu/6.828/2018/xv6/book-rev10.pdf)  
【4】 《Computer Systems: A Programmer's Perspective (3rd Edition)》.Randal E.Bryant / David O'Hallaron.  
【5】 《Modern Opeation Systems (Fourth Edition)》.Andrew S.Tanenbaum / Herbert Bos.  
