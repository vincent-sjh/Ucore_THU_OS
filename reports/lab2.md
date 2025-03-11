# lab2

## 功能实现

修改sys_gettimeofday与sys_trace为页表版本，使用copyin/copyout函数进行操作。

```c
uint64 sys_gettimeofday(TimeVal *val, int _tz) // TODO: implement sys_gettimeofday in pagetable. (VA to PA)
{
	// YOUR CODE
	TimeVal val_buffer;
	uint64 cycle = get_cycle();
	val_buffer.sec = cycle / CPU_FREQ;
	val_buffer.usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	copyout(curr_proc()->pagetable, (uint64)val, (char *)&val_buffer, sizeof(TimeVal));

	/* The code in `ch3` will leads to memory bugs*/

	// uint64 cycle = get_cycle();
	// val->sec = cycle / CPU_FREQ;
	// val->usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	return 0;
}

int sys_trace(int trace_request, unsigned long id, uint8 data)
{
	if(trace_request==0){
		uint8 val;
		pte_t *pte = walk(curr_proc()->pagetable,(uint64)id, 0);
		if(pte == 0){
			return -1;
		}
		// 权限校验 
		int pte_valid =!(*pte & PTE_V);
		int pte_user =!(*pte & PTE_U);
		int pte_read =!(*pte & PTE_R);
		if(pte_valid || pte_user || pte_read){
			return -1;
		}
		int success = copyin(curr_proc()->pagetable, (char*)&val, (uint64)id, sizeof(uint8));
		if(success == -1){
			return -1;
		}
		return val;
	}
	if(trace_request==1){
		pte_t *pte = walk(curr_proc()->pagetable,(uint64)id, 0);
		if(pte == 0){
			return -1;
		}
		// 权限校验
		int pte_valid =!(*pte & PTE_V);
		int pte_user =!(*pte & PTE_U);
		int pte_write =!(*pte & PTE_W);
		if(pte_valid || pte_user || pte_write){
			return -1;
		}
		int success = copyout(curr_proc()->pagetable, (uint64)id, (char*)&data, sizeof(uint8));
		if(success == -1){
			return -1;
		}
		return 0;
	}
	if(trace_request==2){
		return curr_proc()->syscall_num[id];
	}
	return -1;
}
```

在syscall函数中添加sys_mmap和sys_munmap.

```c
void syscall()
{
	//..

	switch (id) {
	//..
	case SYS_mmap:
		ret = sys_mmap((void*)args[0], args[1], args[2],
			       args[3]);
		break;
	case SYS_munmap:
		ret = sys_munmap((void*)args[0], args[1]);
		break;
	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}
	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}

```

实现sys_mmap与sys_munmap，注意各种错误情况返回-1.

```c

int sys_mmap(void* start, unsigned long long len, int prot, int flags)
{
	uint64 addr_begin = (uint64)start;
	if (len == 0) {
		return 0;
	}
	// Alignment
	if ((addr_begin & (PAGE_SIZE - 1)) != 0) {
		return -1;
	}
	// At least one of PROT_READ, PROT_WRITE or PROT_EXEC must be set
	if ((prot & 0x7) == 0) {
		return -1;
	}
	// Other bits are not allowed
	if ((prot & ~0x7) != 0) {
		return -1;
	}

	uint64 addr_end = addr_begin + PGROUNDUP(len);
	for (uint64 addr_virtual = addr_begin; addr_virtual != addr_end; addr_virtual += PAGE_SIZE) {
		void *addr_physical = kalloc();
		if (addr_physical == 0) {
			return -1;
		}
		int perm = (prot << 1) | PTE_U;
		int success = mappages(curr_proc()->pagetable, (uint64)addr_virtual, (uint64)PAGE_SIZE, (uint64)addr_physical,perm);
		if (success != 0) {
			return -1;
		}
	}
	return 0;
}
int sys_munmap(void* start, unsigned long long len)
{
	uint64 addr_begin = (uint64)start;
	if (len == 0) {
		return 0;
	}
	// Alignment
	if ((addr_begin & (PAGE_SIZE - 1)) != 0) {
		return -1;
	}
	uint64 addr_end = addr_begin + PGROUNDUP(len);
	for (uint64 addr_virtual = addr_begin; addr_virtual != addr_end; addr_virtual += PAGE_SIZE) {
		uint64 addr_physical = walkaddr(curr_proc()->pagetable, addr_virtual);
		if (addr_physical == 0) {
			return -1;
		}
		uvmunmap(curr_proc()->pagetable, addr_virtual, 1, 1);
	}
	return 0;
}
```

## 问答题

1. 请列举 SV39 页表页表项的组成，结合课堂内容，描述其中的标志位有何作用／潜在作用？

   SV39 是 RISC-V 架构中定义的一种虚拟内存分页模式，使用 39 位虚拟地址和三级页表结构。SV39 页表项（PTE，Page Table Entry）的大小是 64 位（8 字节），其中包含若干字段和标志位，用于描述页面映射和权限等信息。

---

**SV39 页表项的组成**

**SV39 的页表项是一个 64 位的数据结构，具体格式如下（从高位到低位）：**

| **位域**   | **位数** | **描述**                                                          |
| ---------------- | -------------- | ----------------------------------------------------------------------- |
| **63–54** | **10**   | **保留位（Reserved）**：留给未来扩展使用，目前必须为 0。          |
| **53–10** | **44**   | **物理页面号（PPN，Physical Page Number）**：指向物理页面的地址。 |
| **9–8**   | **2**    | **保留位（Rsvd）或自定义位**：硬件上未定义，可供软件自定义使用。  |
| **7**      | **1**    | **D（Dirty）**：脏页标志，表示页面是否被修改。                    |
| **6**      | **1**    | **A（Accessed）**：访问标志，表示页面是否被访问。                 |
| **5**      | **1**    | **G（Global）**：全局标志，表示页面是否对所有地址空间有效。       |
| **4**      | **1**    | **U（User）**：用户模式标志，表示页面是否允许用户态访问。         |
| **3**      | **1**    | **X（Executable）**：可执行标志，表示页面是否可执行指令。         |
| **2**      | **1**    | **W（Writable）**：可写标志，表示页面是否可写。                   |
| **1**      | **1**    | **R（Readable）**：可读标志，表示页面是否可读。                   |
| **0**      | **1**    | **V（Valid）**：有效标志，表示页表项是否有效。                    |

2. 缺页

   > 这次的实验没有涉及到缺页有点遗憾，主要是缺页难以测试，而且更多的是一种优化，不符合这次实验的核心理念，所以这里补两道小题。
   >
   > 缺页指的是进程访问页面时页面不在页表中或在页表中无效的现象，此时 MMU 将会返回一个中断，告知 os 进程内存访问出了问题。os 选择填补页表并重新执行异常指令或者杀死进程。
   >
   > * 请问哪些异常可能是缺页导致的？
   > * 指令页面错误（Instruction Page Fault）
   > * 加载页面错误（Load Page Fault）
   > * **存储页面错误（Store/AMO Page Fault）**
   > * 发生缺页时，描述相关的重要寄存器的值（lab2中描述过的可以简单点）
   > * `sscratch` 寄存器内是发生缺页的进程的 `trapframe` 地址
   > * `satp` 是发生缺页的进程的页表的地址；
   > * `sepc` 是发生缺页的指令的地址
   >
   > 缺页有两个常见的原因，其一是 Lazy 策略，也就是直到内存页面被访问才实际进行页表操作。比如，一个程序被执行时，进程的代码段理论上需要从磁盘加载到内存。但是 os 并不会马上这样做，而是会保存 .text 段在磁盘的位置信息，在这些代码第一次被执行时才完成从磁盘的加载操作。
   >
   > * 这样做有哪些好处？
   > * 节省内存资源
   > * 加快进程启动速度
   > * 优化磁盘IO
   > * 支持动态内存需求
   > * 便于实现高级内存管理技术
   > * 减少初始化开销
   >
   > 此外 COW(Copy On Write) 也是常见的容易导致缺页的 Lazy 策略，这个之后再说。其实，我们的 mmap 也可以采取 Lazy 策略，比如：一个用户进程先后申请了 10G 的内存空间，然后用了其中 1M 就直接退出了。按照现在的做法，我们显然亏大了，进行了很多没有意义的页表操作。
   >
   > * 请简单思考如何才能在现有框架基础上实现 Lazy 策略，缺页时又如何处理？描述合理即可，不需要考虑实现。
   > * **在现有框架基础上实现 Lazy 策略，可以在用户进程调用 **mmap** 请求内存时，让操作系统只分配虚拟地址空间并记录相关元数据（如大小和来源），而不立即分配物理页面或填充页表，而是将对应页表项标记为无效（V = 0），推迟所有实际操作到页面被访问时。发生缺页时，MMU 检测到无效页表项后触发异常，操作系统根据异常地址检查进程的虚拟内存区域，确认是 Lazy 分配的范围后，从磁盘加载数据（如果是文件映射）或分配零初始化页面（如果是匿名内存），然后更新页表项为有效（设置 PPN 和权限位），最后返回继续执行原指令，这样就实现了按需分配，减少了无意义的资源占用。**
   > * 请问处理 10G 连续的内存页面，需要操作的页表实际大致占用多少内存(给出数量级即可)？
   > * **总页面数**：
   >
   >   * **10GB = 10 × 2^30 字节。**
   >   * **页面数 = 10 × 2^30 / 2^12 = 10 × 2^18 ≈ 2,621,440 个页面。**
   > * **第三级页表（叶子级）需求**：
   >
   >   * **每个第三级页表容纳 512 个 PTE（对应 512 × 4KB = 2MB 内存）。**
   >   * **所需第三级页表数 = 2,621,440 / 512 ≈ 5,122 个。**
   > * **第二级页表需求**：
   >
   >   * **每个第二级页表管理 512 个第三级页表（对应 512 × 2MB = 1GB 内存）。**
   >   * **所需第二级页表数 = 5,122 / 512 ≈ 10 个。**
   > * **第一级页表（根级）需求**：
   >
   >   * **每个第一级页表管理 512 个第二级页表（对应 512 × 1GB = 512GB 虚拟地址空间）。**
   >   * **10GB < 512GB，只需 1 个根页表。**
   > * **总内存占用**：
   >
   >   * **第三级页表：5,122 × 4KB ≈ 20.5MB。**
   >   * **第二级页表：10 × 4KB = 40KB。**
   >   * **第一级页表：1 × 4KB = 4KB。**
   >   * **总计 ≈ 20.5MB + 40KB + 4KB ≈ **20.5MB**。**
   >
   > 缺页的另一个常见原因是 swap 策略，也就是内存页面可能被换到磁盘上了，导致对应页面失效。
   >
   > * 此时页面失效如何表现在页表项(PTE)上？
   > * **在 **swap** 策略下，当内存页面被换到磁盘上时，页表项（PTE）会将 V 位（Valid）置为 0，表示该页面在物理内存中无效，MMU 无法直接访问，同时 PPN（物理页面号）字段可能被操作系统重用来存储页面在交换分区中的偏移量或其他元数据，而 R/W/X 等权限位通常保留换出前的状态以便后续恢复，A/D 位可能被清零因为页面已不在内存，这样当进程访问该页面时会触发缺页异常，操作系统通过检查 PTE 的 V = 0 和 PPN 中的元数据定位磁盘上的页面并换回内存，再更新 PTE 为新的物理页面地址并将 V 设为 1。**
   >
3. 双页表与单页表
   为了防范侧信道攻击，我们的 os 使用了双页表。但是传统的设计一直是单页表的，也就是说，用户线程和对应的内核线程共用同一张页表，只不过内核对应的地址只允许在内核态访问。请结合课堂知识回答如下问题：(备注：这里的单/双的说法仅为自创的通俗说法，并无这个名词概念，详情见 [KPTI](https://en.wikipedia.org/wiki/Kernel_page-table_isolation) )

   * 单页表情况下，如何更换页表？
   * **在单页表情况下，更换页表通常发生在进程切换时，因为每个进程都有自己独立的虚拟地址空间，而页表定义了虚拟地址到物理地址的映射关系。当操作系统调度一个新进程运行时，会将当前进程的页表替换为新进程的页表，具体是通过更新 **satp** 寄存器（在 RISC-V 中）或类似的页表基址寄存器（如 x86 的 CR3），指向新进程的根页表物理地址，同时可能需要刷新 TLB 以确保硬件使用新的映射关系，这种更换是上下文切换的一部分，确保每个进程的内存隔离。**
   * 单页表情况下，如何控制用户态无法访问内核页面？（tips:看看第一题最后一问）
   * 在单页表设计中，用户态无法访问内核页面是通过页表项（PTE）的权限位实现的，具体来说，内核的页面在页表中会被标记为 U = 0（User 位为 0），表示只有特权模式（如内核态 S-mode）可以访问，而用户态（U-mode）访问时会触发页面错误，例如加载页面错误（Load Page Fault）。这种控制依赖于硬件的权限检查机制，MMU 在每次内存访问时会结合当前的运行模式和 PTE 的 U 位判断是否允许访问，从而保证用户线程无法直接触碰内核地址空间的内容。
   * 单页表有何优势？（回答合理即可）
   * **单页表设计的优势在于简单高效，因为用户态和内核态共享同一张页表，进入内核（如系统调用或中断）时无需切换页表，只需改变特权模式即可访问内核空间，这减少了页表切换的开销（例如无需刷新 TLB 或更新 **satp**），尤其在频繁的内核交互场景下性能更高。此外，单页表节省了内存，因为不需要为每个进程维护两套页表结构，只需一套即可覆盖用户和内核的映射，管理起来也更直接，适合资源受限或追求低延迟的系统。**
   * 双页表实现下，何时需要更换页表？假设你写一个单页表操作系统，你会选择何时更换页表（回答合理即可）？
   * **在双页表实现（如 KPTI）下，需要在用户态和内核态之间切换页表，具体来说，每次从用户态进入内核态（如系统调用或中断）时，操作系统会将页表从用户页表切换到内核页表，以加载完整的内核映射，而从内核态返回用户态时再切换回用户页表，只映射必要的最小内核页面（例如异常处理入口），这种切换是为了防范侧信道攻击（如 Meltdown），隔离内核地址空间。如果我设计一个单页表操作系统，我会在进程创建或销毁时更换页表，例如 fork 时为新进程分配一个独立的页表，并在进程退出时释放，同时在调度切换进程时更新 **satp** 指向新页表，这样保持单页表的高效性，同时确保进程间隔离，至于内核态和用户态的切换则完全依赖权限位控制，无需额外更换页表。**

## 荣誉准则

1. 在完成本次实验的过程（含此前学习的过程）中，我曾分别与 **以下各位** 就（与本次实验相关的）以下方面做过交流，还在代码中对应的位置以注释形式记录了具体的交流对象及内容：

   > 无
   >
2. 此外，我也参考了 **以下资料** ，还在代码中对应的位置以注释形式记录了具体的参考来源及内容：

   > 无
   >
3. 我独立完成了本次实验除以上方面之外的所有工作，包括代码与文档。 我清楚地知道，从以上方面获得的信息在一定程度上降低了实验难度，可能会影响起评分。
4. 我从未使用过他人的代码，不管是原封不动地复制，还是经过了某些等价转换。 我未曾也不会向他人（含此后各届同学）复制或公开我的实验代码，我有义务妥善保管好它们。 我提交至本实验的评测系统的代码，均无意于破坏或妨碍任何计算机系统的正常运转。 我清楚地知道，以上情况均为本课程纪律所禁止，若违反，对应的实验成绩将按“-100”分计。
