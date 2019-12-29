
# MIT-6.828-LAB1 Booting a PC

date: 2019-12-28 22:28:42

---

# <span>文章目录</span>

* [Part 1:PC Bootstrap](#oselab1_1)
	* [Exercise 2](#oselab1_1.2)
		* [Key point](#oselab1_1.2.1)
		* [My answer for exercise 2](#oselab1_1.2.2)
* [Part 2:The Boot Loader](#oselab1_2)
	* [Exercise 3](#oselab1_2.3)
		* [Key point](#oselab1_2.3.1)
		* [My answer for exercise 3](#oselab1_2.3.2)
	* [Exercise 4,5,6](#oselab1_2.4)
		* [Key point](#oselab1_2.4.1)
		* [My answer for exercise 4](#oselab1_2.4.2)
		* [My answer for exercise 5](#oselab1_2.4.3)
		* [My answer for exercise 6](#oselab1_2.4.4)
* [Part 3：The Kernel](#oselab1_3)
	* [Exercise 7](#oselab1_3.7)
		* [Key point](#oselab1_3.7.1)
		* [My answer for exercise 7](#oselab1_3.7.2)
	* [Exercise 8](#oselab1_3.8)
		* [My answer for exercise 8](#oselab1_3.8.1)
		* [GDB debugging process for exercise 8.3](#oselab1_3.8.2)
	* [Exercise 9](#oselab1_3.9)
		* [My answer for exercise 9](#oselab1_3.9.1)
	* [Exercise 10](#oselab1_3.10)
		* [Key point](#oselab1_3.10.1)
		* [My answer for exercise 10](#oselab1_3.10.2)
	* [Exercise 11,12](#oselab1_3.11)
		* [My answer for exercise 11,12](#oselab1_3.11.1)
* [参考资料](#oselab1_4)

---

# Part 1:PC Bootstrap<span id="oselab1_1"></span>

---

## <span id="oselab1_1.2">Exercise 2</span>
### <span id="oselab1_1.2.1">Key point</span>
>**When the BIOS runs, it sets up an interrupt descriptor table and initializes various devices** such as the VGA display. This is where the "Starting SeaBIOS" message you see in the QEMU window comes from.  
**After initializing the PCI bus and all the important devices the BIOS knows about, it searches for a bootable device** such as a floppy, hard drive, or CD-ROM. Eventually, when it finds a bootable disk, **the BIOS reads the boot loader from the disk and transfers control to it.**

<!--more-->

什么是实模式real mode?什么是保护模式protected mode？它们的区别是什么？根据MIT官方文档的指示，参阅其给出的pc-asm-book.pdf的1.2.6等：  
>**In real mode,a selector value is a paragragh number of physical memory.In protected mode,a selector value is an index into a descriptor table.**In both modes, programs are divided into segments. In real mode, these segments are at fixed positions in physical memory and the selector value denotes the paragraph number of the beginning of the segment. In protected mode, the segments are not at fixed positions in physical memory. In fact, they do not have to be in memory at all!Protected mode uses a technique called **virtual memory**.

---

### <span id="oselab1_1.2.2">My answer for exercise 2</span>

**注：下面会涉及到许多IO端口号，我查询了Wiki百科，找到[I/O_PORTS](https://wiki.osdev.org/I/O_Ports)，该页面简单介绍了各地址范围的I/O端口的作用，在其底部的外部链接中[Boch's map of I/O ports to functions](http://bochs.sourceforge.net/techspec/PORTS.LST)中则是各I/O端口的详细说明，当然也可以参考Mit给的参考文档[Phil Storrs I/O Ports Description](http://web.archive.org/web/20040404164813/members.iweb.net.au/~pstorr/pcbook/book2/book2.htm),但是不太详细**
```asm
cli # 屏蔽了所有中断
cld # 操作方向标志位DF,使DF复位为0
mov $0x8f,%eax # 0x8f <=> 1000 1111
out %al,$0x70  # 将%al中的一个字节写入0x70端口
in $0x71,%al   # 从IO端口0x71读取一个字节
```
该IO端口0x70是一个CMOS RAM/RTC(Real Time Clock),通过查询上面所说的页面可以发现，0x8f表明其禁止了NMI中断，并且选择了CMOS register number为0xf

```asm
in 0x92,%al   # 从0x92端口读取一个字节
or $0x2,%al   # 将上面读出的字节的第二位设置为1,激活A20地址线
out %al,$0x92 # 将该字节重新写入0x92端口
```
查阅得知，0x92是PS/2 system control port A，并且激活A20地址线，关于A20，建议阅读[Quora:What is the A20 gate in a CPU](https://www.quora.com/What-is-the-A20-gate-in-a-CPU)，通过该回答大致了解到A20是为了向后兼容8086等时仅有20条地址线的情况，并且在操作系统需要进入到protected mode时，A20 Gate应当被使能,但是值得注意的是此时仍然工作在实模式下。
```
lidtw %cs:0x6ab8 # 加载IDT的24位基地址和16位限长值到IDTR寄存器
lgdtw %cs:0x6a74 # 加载GDT的24位基地址和16位限长值到GDTR寄存器
```
因为是`lidtw`，随后这两行仍然是16bit操作数，参考[x86-lidt and lgdt](https://www.fermimn.edu.it/linux/quarta/x86/lgdt.htm)可知，若为16位操作数，那么lidt(lgdt)会将16位限长值与24位基地址加载到寄存器IDTR(GDTR),建议阅读[百度百科：中断描述符表IDT](https://baike.baidu.com/item/%E4%B8%AD%E6%96%AD%E6%8F%8F%E8%BF%B0%E7%AC%A6%E8%A1%A8)以及[Wiki:GDT](https://en.wikibooks.org/wiki/X86_Assembly/Global_Descriptor_Table),因此简单来说，这两行就是加载IDT和GDT。  

```asm
mov %cr0,%eax
or  $0x1,%eax  # 将%cr0寄存器中的值的最低位设置为1
mov %eax,%cr0  # 处理器进入Protected mode
```
上面三行很显然是将%cr0寄存器中的值的最低位设置为1，参阅[Wiki:Control register](https://en.wikipedia.org/wiki/Control_register#CR0)发现，这个操作就是使处理器进入Protected mode。  
```asm
ljmpl $0x8,$0xfd18f  # 这里这样跳转是x86加载GDT之后的硬性规定
mov $0x10,%eax
mov %eax,%ds         # 设置段寄存器
mov %eax,%es         # ~
mov %eax,%ss         # ~
mov %eax,%fs         # ~
mov %eax,%gs         # ~
mov %ecx,%eax
jmp *%edx            # 间接跳转
```
同样参阅[Wiki:GDT](https://en.wikibooks.org/wiki/X86_Assembly/Global_Descriptor_Table)，能了解到，加载了GDT之后，必须通过长跳转（far jump)来重新加载段寄存器，也就是DS,ES,SS,FS,GS。  

从`jmp *%edx`开始，后面的汇编代码就是很类似于C语言编译后的汇编代码，并且后续汇编代码极其之长，但是通过`si`单步调试，发现基本都是很类似的结构，例如我把后续汇编代码截取了一部分，会发现二者很相似。  
```asm
=> 0xf34c2:	push   %ebx
=> 0xf34c3:	sub    $0x2c,%esp
=> 0xf34c6:	movl   $0xf5b5c,0x4(%esp)
=> 0xf34ce:	movl   $0xf447b,(%esp)
=> 0xf34d5:	call   0xf099e
=> 0xf099e:	lea    0x8(%esp),%ecx
=> 0xf09a2:	mov    0x4(%esp),%edx
=> 0xf09a6:	mov    $0xf5b58,%eax
=> 0xf09ab:	call   0xf0574
=> 0xf0574:	push   %ebp 
=> 0xf0575:	push   %edi
=> 0xf0576:	push   %esi
=> 0xf0577:	push   %ebx 
=> 0xf0578:	sub    $0xc,%esp
=> 0xf057b:	mov    %eax,0x4(%esp)
=> 0xf057f:	mov    %edx,%ebp
=> 0xf0581:	mov    %ecx,%esi 

=> 0xf0583:	movsbl 0x0(%ebp),%edx     # 这里开始到ret都是重复代码
=> 0xf0587:	test   %dl,%dl 
=> 0xf0589:	je     0xf0758
=> 0xf058f:	cmp    $0x25,%dl
=> 0xf0592:	jne    0xf0741
=> 0xf0741:	mov    0x4(%esp),%eax
=> 0xf0745:	call   0xefc70
=> 0xefc70:	mov    %eax,%ecx
=> 0xefc72:	movsbl %dl,%edx 
=> 0xefc75:	call   *(%ecx) 
=> 0xefc65:	mov    %edx,%eax
=> 0xefc67:	mov    0xf693c,%dx
=> 0xefc6e:	out    %al,(%dx)          # 向0x402端口写入一字节(0x53)
=> 0xefc6f:	ret    
=> 0xefc77:	ret 
```
上面这段汇编代码在`ret`尚未执行时的完整栈结构如下所示：
 <div align="center">![图1-1 栈示意图](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/parta/lab1partA%E9%83%A8%E5%88%86%E7%A4%BA%E6%84%8F%E6%A0%88.png)</div>

至于为什么我会画出栈？因为我想尝试读懂，结果发现是我想多了。为什么栈是从0x7000地址处开始？因为在整个汇编代码的起始处，%esp寄存器就被赋值为0x7000。

```asm
=> 0xf074a:	mov    %ebp,%ebx
=> 0xf074c:	jmp    0xf0750
=> 0xf0750:	lea    0x1(%ebx),%ebp 
=> 0xf0753:	jmp    0xf0583

=> 0xf0583:	movsbl 0x0(%ebp),%edx     # 这里开始是重复代码
=> 0xf0587:	test   %dl,%dl 
=> 0xf0589:	je     0xf0758
=> 0xf058f:	cmp    $0x25,%dl
=> 0xf0592:	jne    0xf0741
=> 0xf0741:	mov    0x4(%esp),%eax
=> 0xf0745:	call   0xefc70 
=> 0xefc70:	mov    %eax,%ecx) 
=> 0xefc72:	movsbl %dl,%edx 
=> 0xefc75:	call   *(%ecx)
=> 0xefc65:	mov    %edx,%eax 
=> 0xefc67:	mov    0xf693c,%dx
=> 0xefc6e:	out    %al,(%dx)          # 向向0x402端口写入一字节(0x65)
=> 0xefc6f:	ret  
=> 0xefc77:	ret    
```
可以看到随后一段汇编代码，出现了大量的重复，并且最后也是向0x402端口（注：这里`out %al,(%dx)`的语法我并不是很了解，但是通过输出寄存器的值，此时`%dx`当中的确都是0x402。后面有大量的代码都会重复这一段，但是我能力有限，无法知道这一段的具体含义，但是根据MIT在这部分的描述：  
>When the BIOS runs, it sets up an interrupt descriptor table and **initializes various devices such as the VGA display**. 

我只能够**猜测**这一段就是初始化各种BIOS已知的设备。

---

# <span id="oselab1_2">Part 2:The Boot Loader</span>

---

## <span id="oselab1_2.3">Exercise 3</span>

### <span id="oselab1_2.3.1">Key point</span>
>When the BIOS finds a bootable floppy or hard disk, **it loads the 512-byte boot sector into memory at physical addresses 0x7c00 through 0x7dff, and then uses a jmp instruction to set the CS:IP to 0000:7c00, passing control to the boot loader.**(注：0x7c00-0x7dff为512字节，也就是说BIOS将Boot sector加载到0x7c00为起始地址的连续512个字节，并且随后跳转到0x7c00去执行Boot Sector中的代码) Like the BIOS load address, these addresses are fairly arbitrary - but they are fixed and standardized for PCs.  
The boot loader consists of one assembly language source file, **boot/boot.S**, and one C source file, **boot/main.c**  
The boot loader must perform two main functions:  
>1. the boot loader **switches the processor from real mode to 32-bit protected mode**, because it is only in this mode that software can access all the memory above 1MB in the processor's physical address space.
>2. the boot loader **reads the kernel from the hard disk** by directly accessing the IDE disk device registers via the x86's special I/O instructions.

---

### <span id="oselab1_2.3.2">My answer for exercise 3</span>
在这部分当中给出了四个问题需要回答：  
1. At what point does the processor start executing 32-bit code? What exactly causes the switch from 16- to 32-bit mode?
2. What is the last instruction of the boot loader executed, and what is the first instruction of the kernel it just loaded?
3. Where is the first instruction of the kernel?
4. How does the boot loader decide how many sectors it must read in order to fetch the entire kernel from disk? Where does it find this information?  

通过对Part A的汇编代码的仔细研读，再理解boot/boot.S这个文件也就不难了，并且代码中还给了注释，只有一个注意的点：  
```asm
  # Enable A20:
  #   For backwards compatibility with the earliest PCs, physical
  #   address line 20 is tied low, so that addresses higher than
  #   1MB wrap around to zero by default.  This code undoes this.
seta20.1:
  inb     $0x64,%al               # Wait for not busy
  testb   $0x2,%al
  jnz     seta20.1

  movb    $0xd1,%al               # 0xd1 -> port 0x64
  outb    %al,$0x64

seta20.2:
  inb     $0x64,%al               # Wait for not busy
  testb   $0x2,%al
  jnz     seta20.2

  movb    $0xdf,%al               # 0xdf -> port 0x60
  outb    %al,$0x60
```
这一段，直接看注释就能知道它的作用是使能A20 Gate，为切换到保护模式做准备，查询端口表，0x64号端口的bit 1位置的含义是：  
>**0064**: KB controller read status (ISA, EISA)  
>bit 1 = 1 input buffer full (input 60/64 has data for 8042)

再查向0x64端口写入0xd1和0xdf有什么作用：  
>**D1	dbl**: write output port. next byte written  to 0060
will be written to the 804x output port; the original IBM AT and many compatibles use bit 1 of the output port to **control the A20 gate**.  
>**DF**	sngl  **enable address line A20** (HP Vectra only???)

由此，上面这段代码就是检测input buffer直到其空闲，然后使能A20 Gate。

通过该文件，能够**回答第一个问题**：  
```asm
  # Switch from real to protected mode, using a bootstrap GDT
  # and segment translation that makes virtual addresses
  # identical to their physical addresses, so that the
  # effective memory map does not change during the switch.
  lgdt    gdtdesc
  movl    %cr0, %eax
  orl     $CR0_PE_ON, %eax
  movl    %eax, %cr0

  # Jump to next instruction, but in 32-bit code segment.
  # Switches processor into 32-bit mode.
  ljmp    $PROT_MODE_CSEG, $protcseg
```
从`ljmp $PROT_MDOE_CSEG, $protcseg`开始，由于设置了%cr0控制寄存器，使得处理器从实模式切换为保护模式，开始执行32-bit code。  

随后，进入`bootmain()`，我们来深究一下`bootmain`这个函数：  
```c
readseg((uint32_t) ELFHDR, SECTSIZE*8, 0);
```
首先，从第一个扇区(sector)读取ELF中的Program Header，（此处建议参考由Mit推荐的参考页面[Wiki:Executable and Linkable Format](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format#Program_header)），读取SECTSIZE * 8个字节的数据，并且将其加载到物理内存地址0x10000，为什么是0x10000，在注释的解释是`scratch space`，直译的话不知道是什么鬼，google translate给的翻译是“暂存空间”，这样就合理多了。

```c
// is this a valid ELF?
if (ELFHDR->e_magic != ELF_MAGIC)
        goto bad;

// load each program segment (ignores ph flags)
ph = (struct Proghdr *) ((uint8_t *) ELFHDR + ELFHDR->e_phoff);
eph = ph + ELFHDR->e_phnum;
```
随后校验所读取的ELF是否有效，有效则计算Program Header的起始地址，以及结束地址，以便后续遍历Program Header并且将每一段（segment)加载到内存当中，因此下面这段代码就是这样的功能,将ph->p_offset出开始的ph->p_memsz个字节读取到以ph->p_pa为起始地址的物理内存地址上：  
```c
for (; ph < eph; ph++)
        // p_pa is the load address of this segment (as well
        // as the physical address)
        readseg(ph->p_pa, ph->p_memsz, ph->p_offset);
```
然后我们进入`readseg`，来看看它是如何做到将扇区的内容加载到内存中的：  
```c
uint32_t end_pa;
end_pa = pa + count;
// round down to sector boundary
pa &= ~(SECTSIZE - 1);
```
首先，计算物理内存地址的结束地址（这里实际上是结束地址的下一个地址）,并且将物理内存地址的起始地址向下舍入到sector boundary(若SECTSIZE是512，其实也就等价于`pa & 0xffffffe0`,那它为什么要这么写，因为这么写有强大的兼容性啊，无论是对32位机器还是64位机器都是适用的。  
```c
offset = (offset / SECTSIZE) + 1;
```
然后将需要读取的字节数，转换为扇区编号。  
```c
while (pa < end_pa) {
        // Since we haven't enabled paging yet and we're using
        // an identity segment mapping (see boot.S), we can
        // use physical addresses directly.  This won't be the
        // case once JOS enables the MMU.
        readsect((uint8_t*) pa, offset);
        pa += SECTSIZE;
        offset++;
}
```
遍历加载每个扇区的内容，注意这里的注释，在此时，页表机制还没加载，因此，这里不是使用虚拟内存，而是直接的物理内存地址。其次，这里出现了`readsect`函数，该函数是Exercise 3要求必须完全弄懂的函数，下面就进入`readsect`来看看它到底做了什么：  
```c
void
waitdisk(void)
{
        // wait for disk reaady
        while ((inb(0x1F7) & 0xC0) != 0x40)
                /* do nothing */;
}


void readsect(void *dst, uint32_t offset)
{
        // wait for disk to be ready
        waitdisk();

        outb(0x1F2, 1);         // count = 1
        outb(0x1F3, offset);
        outb(0x1F4, offset >> 8);
        outb(0x1F5, offset >> 16);
        outb(0x1F6, (offset >> 24) | 0xE0);
        outb(0x1F7, 0x20);      // cmd 0x20 - read sectors

        // wait for disk to be ready
        waitdisk();

        // read a sector
        insl(0x1F0, dst, SECTSIZE/4);
}
```
`readsect`和`waitdisk`直接与硬件交互，向设备端口写入数据，同样查询之前的端口表，这里用到的端口梳理如下： <div align="center">![图2-1 Disk Controller端口梳理](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/lab1partB%E7%A1%AC%E7%9B%98%E8%AF%BB%E5%8F%96%E7%AB%AF%E5%8F%A3%E6%A2%B3%E7%90%86.PNG)</div>

因此，`waitdisk`中期待从0x1F7端口读出的一字节与0xC0相与的结果为0x40，也就是drive is ready，等待硬盘准备完毕。  
后续`outb(0x1F2, 1);`选择需要读取的扇区数量为1，`outb(0x1F3, offset);`选择扇区编号，向0x1F4,0x1F5,0x1F6写入的数据应该是操作disk选择相应的柱面、磁道等，然后`outb(0x1F7, 0x20);`读取扇区数据，随后等待磁盘准备完毕，再从磁盘数据读取到指定的物理内存地址dst。  
```c
static inline void
insl(int port, void *addr, int cnt)
{
        asm volatile("cld\n\trepne\n\tinsl"
    7cc9:       8b 7d 08                mov    0x8(%ebp),%edi
    7ccc:       b9 80 00 00 00          mov    $0x80,%ecx
    7cd1:       ba f0 01 00 00          mov    $0x1f0,%edx
    7cd6:       fc                      cld
    7cd7:       f2 6d                   repnz insl (%dx),%es:(%edi)

        // read a sector
        insl(0x1F0, dst, SECTSIZE/4);
}
```
`insl`直接采用了在c中嵌入汇编代码的形式，前三条mov指令准备函数参数，最核心的就是`repnz insl (%dx),%es:(%edi)`，通过查阅Mit给的i386.pdf：  
>REPNE/REPNZ ── **Repeat while not equal or not zero**  
>The primitive string operations operate on one element of a string. A string element may be a byte, a word, or a doubleword. The string elements are addressed by the registers ESI and EDI. After every primitive operation ESI and/or EDI are automatically updated to point to the next element of the string. If the direction flag is zero, the index registers are incremented; if one, they are decremented. The amount of the increment or decrement is 1, 2, or 4 depending on the size of the string element.

简而言之，被该指令修饰的指令能够自动重复执行，直到相应的结束条件被满足，这样你也能理解为什么先要执行cld使direction flag复位为0，为的是使index register增长而不是减小，这样就能够将数据按照物理内存地址从到大存放了，虽然能猜出来它做了什么，但是这里对`insl`的具体语意仍然存疑，只能用GDB调试了。  

由于直接看C代码能看懂绝大部分了，因此，这里不按照官方文档提示的将断点打在0x7c00，`vi boot.asm`打开obj/boot.asm文件，`:?insl`找到`insl`函数，可以看到for循环是从0x7d51开始的，`readseg`定义在0x7cdc，`readsect`定义在0x7c7c，而`insl`的第一条指令定义在0x7cc9处，由于我的目的是了解`insl`函数的具体过程，因此将断点打在0x7cc9处，然后单步运行至第一条`repnz insl (%dx),%es:(%edi)`前，输出寄存器内容: <div align="center">![图2-2 第一条`repnz`指令执行之前](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/lab1partB%E6%96%AD%E7%82%B9%E6%89%93%E5%9C%A80x7cc9.PNG)</div>

然后执行第一次`repnz insl (%dx),%es:(%edi)`,再次输出寄存器内容： <div align="center">![图2-3 执行了第一条`repnz`指令之后](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/lab1partB%E6%96%AD%E7%82%B9%E6%89%93%E5%9C%A80x7cc9%E7%9A%84%E5%90%8E%E7%BB%AD.PNG)</div>

很明显了，`insl`指令将4个字节的数据从扇区读出来到0x1f0端口，然后将其存放到以%edi为起始物理内存地址的地方，然后将%edi寄存器中的数据加4，%ecx寄存器中的数据减1，并且会检测%ecx寄存器中是否为0，若为0，则达到了`repnz`终止的条件，换而言之，若的确是这样，那么该指令就会执行128次，因为初始时%ecx寄存器值为128（恰巧，`insl`函数的第三个参数就是128，都对上了！），为了验证的确将数据复制了，输出以0x10000为起始地址的10个双字的数据，再执行9条指令，再次输出寄存器内容以及内存信息： <div align="center">![图2-4 再次输出寄存器内容及内存信息](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/lab1partB%E9%AA%8C%E8%AF%81lnsl%E5%87%BD%E6%95%B0%E5%8A%9F%E8%83%BD.PNG)</div>

此时`repnz insl (%dx),%es:(%edi)`已经执行了10次，应当再执行118次就会终止，`si 117`验证： <div align="center">![图2-5 验证执行了128次](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/lab1partB%E9%AA%8C%E8%AF%81%E6%89%A7%E8%A1%8C%E4%BA%86128%E6%AC%A1.PNG)</div>

从上面的结果完全证明了我的猜测是正确的。后续就是执行相同的循环来读取扇区内容到内存，就不再赘述。  

为了找出Boot Loader执行的最后一条指令以及Kernel执行的第一条指令，将断点打在`readseg`处，随后经过调试，可以发现，执行了4次之后，再单步调试，然后可以找到`bootmain`最后执行的代码`((void (*)(void)) (ELFHDR->e_entry))();`对应的汇编代码： <div align="center">![图2-6 断点打在readseg处，执行了四次后结束](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/lab1partB%E6%89%BEkernel%E5%85%A5%E5%8F%A31.PNG)</div>然后能找到bootmain最后执行的代码对应的汇编代码的入口 <div align="center">![图2-7 bootmain最后执行的代码对应的汇编代码的入口](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/lab1partB%E6%9C%80%E5%90%8E%E7%9A%84%E8%B0%83%E7%94%A8%E5%87%BD%E6%95%B0%E5%85%A5%E5%8F%A3.PNG)</div>

然后继续向下调试，发现最终进入了**kern/entry.S**。 <div align="center">![图2-8 最终进入entry.S开始为初始化内核做准备](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/lab1partB%E6%9C%80%E7%BB%88kernel%E5%85%A5%E5%8F%A3%E6%89%BE%E5%88%B0%E4%BA%86.PNG)</div>

**那么从`call *0x10018`一直到`jmp *%eax`之间的指令做了什么呢？**  
```asm
movw $1234,0x472
```
不是很确定这条指令做什么的，但是当你启动qemu时，`make qemu`会有“sed "s/localhost:1234/localhost:26000/" < .gdbinit.tmpl > .gdbinit”这样的信息，这么巧合的数字，大胆猜测这二者是有联系的。
```asm
mov $0x110000,%eax
mov %eax,%cr3
```
不妨再回去看一看[Wiki:Control register](https://en.wikipedia.org/wiki/Control_register#CR0)，或者如果你看过mit给的xv6-book.pdf,那么你就明白，0x110000即为entrypgdir的物理地址，它与虚拟内存的实现息息相关。  
```asm
mov %cr0,%eax
or  $0x80010001,%eax
mov %eax,%cr0
```
同样参考Wiki页面，明白了吗，这段代码设置了%cr0控制寄存器的PE,WP,PG位，即开启Protected Mode,Write Protect以及Paging,也就是**开启了Paging功能，能够将物理地址映射为虚拟地址**  

**Exercise 3到这里基本告一段落，下面是我对四个问题的解答（若有误烦请指正）：**  
1. 从`ljmp $PROT_MDOE_CSEG, $protcseg`开始，由于设置了%cr0控制寄存器，使得处理器从实模式切换为保护模式，开始执行32-bit code。 
2. Boot loader执行的最后一条指令应该是`call *0x10018`,加载内核之后的第一条指令应该是`movl $0x1234,0x472`
3. 内核的第一条指令位于**kern/entry.S**
4. Boot loader通过ELF的Program Header中存储的各种信息，例如p_paddr,p_memsz,p_offset等可以确定需要加载多少sector。

---

## <span id="oselab1_2.4">Exercise 4,5,6</span>

### <span id="oselab1_2.4.1">key point</span>
>An ELF binary starts with a fixed-length ELF header, followed by a variable-length program header listing each of the program sections to be loaded. The C definitions for these ELF headers are in inc/elf.h. The program sections we're interested in are:
>* .text: The program's executable instructions.
>* .rodata: Read-only data, such as ASCII string constants produced by the C compiler. (We will not bother setting up the hardware to prohibit writing, however.)
>* .data: The data section holds the program's initialized data, such as global variables declared with initializers like int x = 5;

---
### <span id="oselab1_2.4.2">My answer for exercise 4</span>
关于C指针的练习我就不写了。  
但是按照文档要求，反汇编了一下 obj/kern/kernel,obj/boot/boot.out,obj/kern/kernel三个文件: <div align="center">![图2-9 kernel的ELF Header](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/exercise4/lab1partBexercise4%E7%9A%84kernelELF.PNG)</div>

将kernel的代码加载到物理内存地址0x100000对应于虚拟地址0xf0100000。 <div align="center">![图2-10 boot的ELF Header](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/exercise4/lab1partBexercise4bootELF.PNG)</div>

与kernel不同，boot程序运行时还没有虚拟内存，页面等机制，因此，虚拟地址就是物理内存地址。 <div align="center">![图2-11 kernel ELF Header的Program Header](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/exercise4/lab1partBexercise4ProgramHeader.PNG)</div>

这个就是Kernel的Program Header,LOAD标识的segment会被加载到内存当中。

### <span id="oselab1_2.4.3">My answer for exercise 5</span>
将Makefrag中的boot loader的link address修改为0x7c2d,然后`make clean`清除之前的编译信息，`make`重新编译后，再用GDB调试，发现这次，程序会一直卡在`ljmp`指令上无法运行了： <div align="center">![图2-12 修改了boot loader的链接地址后程序出错](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/exercise5/lab1partBexercise5%E4%BF%AE%E6%94%B9%E4%BA%86bootloader%E9%93%BE%E6%8E%A5%E5%9C%B0%E5%9D%80.PNG)</div>

### <span id="oselab1_2.4.4">My answer for exercise 6</span>
问题是：  
>Reset the machine (exit QEMU/GDB and start them again). Examine the 8 words of memory at 0x00100000 at the point the BIOS enters the boot loader, and then again at the point the boot loader enters the kernel. Why are they different? What is there at the second breakpoint?

其实很好解释，参考exercise 4反汇编的kernel的.text节，当boot loader进入kernel的时候，此时0x100000起始的物理内存空间已经被加载了内核代码，而当BIOS进入boot loader时，BIOS只是将boot loader的代码加载到了0x7c00起始的物理内存空间，并且此时还运行在real mode之下，那么这时候输出0x100000内存地址的内容，当然是0，不妨验证一下：<div align="center">![图2-13 对exercise6猜测的验证](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/exercise6/%E9%AA%8C%E8%AF%81.PNG)</div>

---

# <span id="oselab1_3">Part 3：The Kernel</span>

---

## <span id="oselab1_3.7">Exercise 7</span>
### <span id="oselab1_3.7.1">Key point</span>
>Many machines don't have any physical memory at address 0xf0100000, so we can't count on being able to store the kernel there. Instead, we will use the processor's memory management hardware to **map virtual address 0xf0100000 (the link address at which the kernel code expects to run) to physical address 0x00100000** (where the boot loader loaded the kernel into physical memory). This way, **although the kernel's virtual address is high enough to leave plenty of address space for user processes, it will be loaded in physical memory at the 1MB point in the PC's RAM, just above the BIOS ROM.**  
>In fact, in the next lab, **we will map the entire bottom 256MB of the PC's physical address space, from physical addresses 0x00000000 through 0x0fffffff, to virtual addresses 0xf0000000 through 0xffffffff respectively. You should now see why JOS can only use the first 256MB of physical memory.**  
>For now, we'll just map the first **4MB** of physical memory.

### <span id="oselab1_3.7.2">My answer for exercise 7</span>
事实上，在exercise3当中，通过对%cr0控制寄存器各bit作用的说明，就已经知道`movl %eax, %cr0`这条指令最终设置了PE,WP,PG位，最重要的是启用了Paging功能，能够将虚拟地址映射到物理地址，按照exercise 7的要求，进行gdb调试。 <div align="center">![图3-1 验证`mov %eax,%cr0`开启了Paging](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/exercise7/%E9%AA%8C%E8%AF%81%E5%85%B6%E5%BC%80%E5%90%AF%E4%BA%86%E8%99%9A%E6%8B%9F%E5%86%85%E5%AD%98%E5%92%8C%E9%A1%B5%E8%A1%A8%E6%9C%BA%E5%88%B6.PNG)</div>

可以发现，在这一行开启了Paging功能之后，0xf0100000虚拟内存地址被映射到了物理地址0x100000，倘若我们注释掉`movl %eax, %cr0`，很显然，此时没有开启Paging，没有映射关系，那么此时一定无法执行后续指令，不妨验证一下： <div align="center">![图3-2 注释掉`mov %eax,%cr0`后代码出错](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/exercise7/%E6%B3%A8%E9%87%8A%E6%8E%89%E5%BC%80%E5%90%AFpaging%E7%9A%84%E9%82%A3%E4%B8%80%E8%A1%8C%E4%B9%8B%E5%90%8E%E4%BB%A3%E7%A0%81%E5%87%BA%E9%94%99.PNG)</div>

可以看到注释了那一行之后，直接导致qemu出错终止了！

---

## <span id="oselab1_3.8">Exercise 8</span>
### <span id="oselab1_3.8.1">My answer for exercise 8</span>
修改为（然后`make qemu`可以验证输出结果）:  
```c
num = getuint(&ap, lflag);
base = 8;
goto number;
```
1. Explain the interface between printf.c and console.c. Specifically, what function does console.c export? How is this function used by printf.c?
	* printf.c调用了console.c中定义的`cputchar(int c)`，用它输出字符c到屏幕上。
2. Explain the following from console.c:
	* crt_pos指向字符当前位置（相对于屏幕而言的），CRT_COLS为一行能容纳字符的大小，CRT_SIZE是整个屏幕能容纳的字符。当屏幕上的字符已经满了的时候（比如整个屏幕都是文字的时候，你再打字，是不是最上面一行就会消失并且下面空出了一行，这里就是做这种操作），因此`memmove(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t))`给我的感觉很像“滑动窗口”的感觉，简而言之就是屏幕将crt_buf+CRT_COLS起始处的(CRT_SIZE-CRT_COLS)个字符显示出来，后续for循环不断执行`crt_buf[i] = 0x0700 | ' ';`将最后新出现的一行填充上空格(这里0x0700是用来控制字符的样式的，这里0x0700是黑底白字，但因为是空格，不会有效果），这时候crt_pos应该在屏幕的最后位置，于是`crt_pos -= CRT_COLS;`将其移动到最后一行的开头处。
3. For the following questions you might wish to consult the notes for Lecture 2. These notes cover GCC's calling convention on the x86.Trace the execution of the following code step-by-step:
	* 3.1:In the call to cprintf(), to what does fmt point? To what does ap point?
		* fmt指向格式字符串"x %d, y %x, z %d\n"（这部分存储在ELF Header的.rodata当中，因此在实际进程中，其内存地址不在栈中，而是在text and data那一部分),ap指向x，也即存储的是x的地址。
	* 3.2:List (in order of execution) each call to cons_putc, va_arg, and vcprintf. For cons_putc, list its argument as well. For va_arg, list what ap points to before and after the call. For vcprintf list the values of its two arguments.
		* 直接将代码添加在kern/init.c当中，为了方便查看obj/kern/kernel.asm时找到我们添加的代码，在代码中使用"exercise8_for_lab1:"标记好，然后在`cons_putc`,`vcprintf`以及`va_arg`的函数入口地址处打上断点（注：由于编译器的优化，会将getint和getuint优化成inline function，因此必须取消这种优化才便于调试，将static删除），调试过程见下方GDB调试过程。以下为完整答案：<center>![图3-3 8.3.2参考答案示意图](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/exercise8/exercise8_3.PNG)</center>
4. （题目略）代码输出结果是"He110 Worldentering test_backtrace 5",因为57616的16进制表示为0xe110,而由于x86是小端机器，因此`unsigned int i=0x00646c72`中的i在内存中（内存地址从小到大）为"72 6c 64 00",查询ASCII表可知结果对应为"rld\0"。相反如果是大端机器，则需要内存中布局不变，那么就需把代码改成`unsigned int i=0x726c6400`,而57616不需要改变。
5. （题目略）按照8.3的方法添加代码，执行`make qemu`，我的结果是"x=3 y=1600"。按照调试8.3的经验，1600(0x640)应该是紧接着3所在的内存地址处的值,验证见下图,可以看到0x3处紧邻的0x640即为y的输出值: <div align="center">![图3-4 验证8.5](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/exercise8/%E9%AA%8C%E8%AF%81exercise8%E7%9A%845.PNG)</div>

6. （题目略）x86完全使用栈来传递函数参数，并且从上面的调试过程我们能发现，x86的函数入栈是从右向左入栈的，而6则让我们想一想若将参数传递改成从左向右（即按照参数的声明顺序），需要改动什么才能实现可变数量的参数。关键问题在于若参数数量不确定，那么被调用函数就没办法在栈中顺利找到传递给它的固定参数值（因为偏移量是不确定的），或许可以在参数都入栈之后，再由编译器在其后存储上实际调用过程中添加的可变参数的数量的值，这样就能够确定偏移量了，额...感觉我的办法有点笨，不过也没有办法验证。
7. **Challenge Enhance the console to allow text to be printed in different colors.**事实上，在前面我们发现console.c中就有设置字符样式的代码，通过改变字符的高16位，来提示CGA字符的样式，默认是`c |= 0x0700;`，尝试将其改为`c |= 0x0600`，结果如下，成功改变颜色！ <div align="center">![图3-5 改变控制台字符颜色](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/exercise8/colorEnhance.PNG)</div>

### <span id="oselab1_3.8.2">GDB debugging process for exercise 8.3</span>

在kernel.asm中找到添加的代码，其地址为0xf01000d3。 <div align="center">![图3-6 在kernel.asm中找到添加的代码](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/exercise8/cprintf%E5%85%A5%E5%8F%A3%E5%A4%84.PNG)</div>

验证0xf0101872处是格式字符串。 <div align="center">![图3-7 验证0xf0101872处是格式字符串](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/exercise8/%E5%9C%B0%E5%9D%800xf0101872%E5%A4%84%E5%8D%B3%E4%B8%BA%E6%A0%BC%E5%BC%8F%E5%AD%97%E7%AC%A6%E4%B8%B2.PNG)</div>

然后查看传递给vcprintf的参数，可以看到为0xf0101872以及0xf010ffd4,8.3.1解决！
 <div align="center">![图3-8 查看传递给vcprintf的参数是什么](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/exercise8/vcprintf%E5%85%A5%E5%8F%A3%E5%8F%82%E6%95%B0%E5%80%BC.PNG)</div>

将断点打在0xf0100300即可找到第一个`cons_putc`函数的入口处。
 <div align="center">![图3-9 第一个cons_putc入口处](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/exercise8/%E7%AC%AC%E4%B8%80%E4%B8%AAcons_putc%E5%85%A5%E5%8F%A3%E5%A4%84.PNG)</div>

发现传递给cons_putc的参数值为0x78,显然，即为字符'x'!
 <div align="center">![图3-10 输出第一个cons_putc的参数](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/exercise8/%E7%AC%AC%E4%B8%80%E4%B8%AAcons_putc%E5%85%A5%E5%8F%A3%E5%A4%84%E7%9A%84%E5%8F%82%E6%95%B0%E5%80%BC.PNG)</div>

由于va_arg是一个宏定义，没办法打断点，只能够将断点打在getuint和getint函数上，然后会发现，每一个va_arg对应的汇编代码大概都是这样，很明显，将ap的值加4后又赋值给ap,例如开始ap为0xf010ffd4,那么通过va_arg宏，ap改变为0xf010ffd8。
 <div align="center">![图3-11 va_arg所起的作用](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/exercise8/%E9%AA%8C%E8%AF%81va_arg%E4%BD%9C%E7%94%A8.PNG)</div>

后续都是重复过程，就不再赘述。

---

## <span id="oselab1_3.9">Exercise 9</span>
### <span id="oselab1_3.9.1">My answer for exercise 9</span>
回想之前几个exercise，应该在entry.S当中初始化栈，结合obj/kern/kernel.asm以及inc/memlayout.h还有inc/mmu.h很容易找到答案：在0xf0100034处`mov $0xf0110000,%esp`初始化内核栈，对应于entry.S中的`mov $(bootstacktop),%esp`,并且在inc/mmu.h中定义了PGSIZE=4096,在inc/memlayout.h中定义了内核栈的大小KSTKSIZE=8*PGSIZE即2^15即0x8000,因此内核栈的内存区域为0xf0110000~0xf0108000.  

---

## <span id="oselab1_3.10">Exercise 10</span>
### <span id="oselab1_3.10.1">Key point</span>
>**The ebp (base pointer) register, in contrast, is associated with the stack primarily by software convention. On entry to a C function, the function's prologue code normally saves the previous function's base pointer by pushing it onto the stack, and then copies the current esp value into ebp for the duration of the function.** If all the functions in a program obey this convention, then at any given point during the program's execution, it is possible to trace back through the stack by following the chain of saved ebp pointers and determining exactly what nested sequence of function calls caused this particular point in the program to be reached. This capability can be particularly useful, for example, when a particular function causes an assert failure or panic because bad arguments were passed to it, but you aren't sure who passed the bad arguments. A stack backtrace lets you find the offending function.

### <span id="oselab1_3.10.2">My answer for exercise 10</span>
`test_backtrace`定义在地址0xf0100040,将断点打在此处，然后调试即可。下图是该递归函数进入到第二层的栈结构，可以得出以下结论，每一层都需要8个字的空间，即8*4=32字节。正如官方文档所说，每个函数调用入口都会保存调用者的栈帧地址%ebp（被调用者保存寄存器）,因此能够轻松地向上回溯到需要的地方，拿到想要的值:
 <div align="center">![图3-12 test_backtrace递归栈示意图](http://q2nelz817.bkt.clouddn.com/blog/tech/articles/mit6.828/lab1/partb/exercise10/%E9%80%92%E5%BD%92%E6%A0%88%E7%A4%BA%E6%84%8F%E5%9B%BE.PNG)</div>

---

## <span id="oselab1_3.11">Exercise 11,12</span>
### <span id="oselab1_3.11.1">My answer for exercise 11 and 12</span>
因为在kern/entry.S中能发现，内核初始化内核栈时，首先执行了`mov $0,%ebp`，因此，栈顶保存的%ebp的值为0，这就是终止条件。代码实现很简单。 在kern/kdebug.c中添加的代码为(其中N_SLINE定义在stab.h当中）：
```c
stab_binsearch(stabs,&lline,&rline,N_SLINE,addr);
	if(lline > rline)return -1;
	info->eip_line = stabs[lline].n_desc;
```
下面就是完整的实现，有两个注意点：
1. 此时不能使用malloc函数，因为还没实现呢！
2. 注意uintptr_t等价于uint32_t，查看inc/type.h就能发现，二者是等价的。

```c
int mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	struct Eipdebuginfo eipInfo;
	uintptr_t ebp = read_ebp(),*t;
	int count = 0;
	while(ebp != 0){
		t = (uintptr_t*)ebp;
		if(debuginfo_eip((uintptr_t)t[1],&eipInfo) >= 0){
			cprintf("ebp %x eip %x args %08x %08x %08x %08x %08x\n", ebp, t[1],t[2],t[3],t[4],t[5],t[6]);
			cprintf("\t%s:%d: %.*s+%d\n",eipInfo.eip_file,eipInfo.eip_line,eipInfo.eip_fn_namelen,eipInfo.eip_fn_name,eipInfo.eip_fn_addr);
			++count;
		}
		ebp=t[0];
	}
	return count;
}
```
最后别忘了在kern/monitor.c当中添加上该指令：
```c
static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "mon_backtrace", "Display the stack frame", mon_backtrace },
};
```

---

# <center><span id="oselab1_4">参考资料</span></center>

---

【1】：[Intel 80386 Reference Programmer's Manual](https://pdos.csail.mit.edu/6.828/2018/readings/i386/toc.htm)  
【2】：[xv6 - a simple, Unix-like teaching operating system - Reference Book](https://pdos.csail.mit.edu/6.828/2018/xv6/book-rev10.pdf)  
【3】：[PC Assembly Language.Paul A. Carter](https://pdos.csail.mit.edu/6.828/2018/readings/pcasm-book.pdf)  
【4】：[Boch's map of I/O ports to functions](http://bochs.sourceforge.net/techspec/PORTS.LST)  
【5】：[Phil Storrs I/O Ports Description](http://web.archive.org/web/20040404164813/members.iweb.net.au/~pstorr/pcbook/book2/book2.htm)  
【6】：[Quora:What is the A20 gate in a CPU](https://www.quora.com/What-is-the-A20-gate-in-a-CPU)  
【7】：[x86-lidt and lgdt](https://www.fermimn.edu.it/linux/quarta/x86/lgdt.htm)  
【8】：[Wiki:GDT](https://en.wikibooks.org/wiki/X86_Assembly/Global_Descriptor_Table)  
【9】：[Wiki:Control register](https://en.wikipedia.org/wiki/Control_register#CR0)  
【10】：[Wiki:Executable and Linkable Format](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format#Program_header)  
【11】：[Phil Storrs PC Hardware book - Understanding the CMOS](http://web.archive.org/web/20040603021346/http://members.iweb.net.au/~pstorr/pcbook/book5/cmos.htm)  
【12】：《Computer Systems: A Programmer's Perspective (3rd Edition)》.Randal E.Bryant / David O'Hallaron.  
【13】：《Modern Opeation Systems (Fourth Edition)》.Andrew S.Tanenbaum / Herbert Bos.  


