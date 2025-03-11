# lab3

宋建昊 2022010853

## 功能实现

```c
//proc.h
struct proc {
	//..
    //添加priority和stride的变量定义
	long long priority;
	long long stride;
};
extern struct proc pool[NPROC];
//proc.c
struct proc *allocproc()
{
	//..
    //初始化priority与stride变量
	p->priority = 16;
	p->stride = 0;
	//..
}

void scheduler()
{
	struct proc *p;
	for (;;) {
		//..
        //算法会在每次调度之后使用priority更新stride
		p->stride += 65536/p->priority;
		//..
	}
}
//queue.c
//实现基于stride调度算法的调度函数
int pop_queue(struct queue *q)
{
	if (q->empty)
		return -1;
	long long prior = q->front;
	long long stride = pool[q->data[q->front]].stride;
	//遍历找到stride最小的进程进行调度
	for (int i = q->front; i != q->tail; i = (i + 1) % NPROC) {
		if (pool[q->data[i]].stride < stride) {
			prior = i;
			stride = pool[q->data[i]].stride;
		}
	}
	int ret = q->data[prior];
	q->data[prior] = q->data[q->front];
	q->front = (q->front + 1) % NPROC;
	if (q->front == q->tail)
		q->empty = 1;
	return ret;

}
//syscall.c
//实现sys_spawn函数
uint64 sys_spawn(uint64 va)
{
	// TODO: your job is to complete the sys call
	char program_name[200];
	copyinstr(curr_proc()->pagetable, program_name, va, 200);
	int id=get_id_by_name(program_name);
	if(id<0){
		return -1;
	}
	struct proc *newproc = allocproc();
	if(newproc== 0){
		return -1;
	}

	loader(id, newproc);
	newproc->parent = curr_proc();
	add_task(newproc);
	return newproc->pid;
}

//实现sys_set_priority函数
uint64 sys_set_priority(long long prio){
    // TODO: your job is to complete the sys call
    if(prio < 2){
		return -1;
	}

	curr_proc()->priority = prio;
	return prio;
}

//修改sys_mmap函数
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
		uint64 max = addr_virtual / PAGE_SIZE;
		if (addr_physical == 0) {
			if(max > curr_proc()->max_page){
				curr_proc()->max_page = max;
			}
			return -1;
		}
		int perm = (prot << 1) | PTE_U;
		int success = mappages(curr_proc()->pagetable, (uint64)addr_virtual, (uint64)PAGE_SIZE, (uint64)addr_physical,perm);
		if (success != 0) {
			if(max > curr_proc()->max_page){
				curr_proc()->max_page = max;
			}
			return -1;
		}
	}
	uint64 max_ = addr_end / PAGE_SIZE;
	if(max_ > curr_proc()->max_page){
		curr_proc()->max_page = max_;
	}
	return 0;

}
```



## 问答题

stride 算法深入

> stride 算法原理非常简单，但是有一个比较大的问题。例如两个 pass = 10 的进程，使用 8bit 无符号整形储存 stride， p1.stride = 255, p2.stride = 250，在 p2 执行一个时间片后，理论上下一次应该 p1 执行。
>
> - 实际情况是轮到 p1 执行吗？为什么？
> - 并不会因为+10后会发生溢出，导致p2的stride变为4，之后还是p2执行。
>
> 我们之前要求进程优先级 >= 2 其实就是为了解决这个问题。可以证明，**在不考虑溢出的情况下**, 在进程优先级全部 >= 2 的情况下，如果严格按照算法执行，那么 STRIDE_MAX – STRIDE_MIN <= BigStride / 2。
>
> - 为什么？尝试简单说明（传达思想即可，不要求严格证明）。
> - 每次stride小的进程会增加不超过BigStride / 2，这导致增加完之后差距最大的两个stride之差也不超过BigStride / 2。
>
> 已知以上结论，**在考虑溢出的情况下**，假设我们通过逐个比较得到 Stride 最小的进程，请设计一个合适的比较函数，用来正确比较两个 Stride 的真正大小：
>
> 如果差值小于BigStride / 2就正常比较，否则将结果取反。
>
> ```
> typedef unsigned long long Stride_t;
> const Stride_t BIG_STRIDE = 0xffffffffffffffffULL;
> 
> int cmp(Stride_t a, Stride_t b) {
>     if (a == b) return 0;
>     Stride_t diff = a - b;  // 无符号减法
>     if (diff <= BIG_STRIDE / 2) return diff > 0 ? 1 : -1;  // 正常范围内，a > b 返回 1，a < b 返回 -1
>     return diff > 0 ? -1 : 1;  // 溢出回绕时，反转结果
> }
> ```
>
> 例子：假设使用 8 bits 储存 stride, BigStride = 255。那么：
>
> - cmp(125, 255) == 1
> - cmp(129, 255) == -1

## 荣誉准则

1. 在完成本次实验的过程（含此前学习的过程）中，我曾分别与 **以下各位** 就（与本次实验相关的）以下方面做过交流，还在代码中对应的位置以注释形式记录了具体的交流对象及内容：

   > 无

2. 此外，我也参考了 **以下资料** ，还在代码中对应的位置以注释形式记录了具体的参考来源及内容：

   > 无

3. 我独立完成了本次实验除以上方面之外的所有工作，包括代码与文档。 我清楚地知道，从以上方面获得的信息在一定程度上降低了实验难度，可能会影响起评分。

4. 我从未使用过他人的代码，不管是原封不动地复制，还是经过了某些等价转换。 我未曾也不会向他人（含此后各届同学）复制或公开我的实验代码，我有义务妥善保管好它们。 我提交至本实验的评测系统的代码，均无意于破坏或妨碍任何计算机系统的正常运转。 我清楚地知道，以上情况均为本课程纪律所禁止，若违反，对应的实验成绩将按“-100”分计。