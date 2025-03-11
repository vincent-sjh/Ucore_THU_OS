#include "syscall.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"
#include "proc.h"

uint64 sys_write(int fd, uint64 va, uint len)
{
	debugf("sys_write fd = %d va = %x, len = %d", fd, va, len);
	if (fd != STDOUT)
		return -1;
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	int size = copyinstr(p->pagetable, str, va, MIN(len, MAX_STR_LEN));
	debugf("size = %d", size);
	for (int i = 0; i < size; ++i) {
		console_putchar(str[i]);
	}
	return size;
}

__attribute__((noreturn)) void sys_exit(int code)
{
	exit(code);
	__builtin_unreachable();
}

uint64 sys_sched_yield()
{
	yield();
	return 0;
}

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

uint64 sys_sbrk(int n)
{
	uint64 addr;
        struct proc *p = curr_proc();
        addr = p->program_brk;
        if(growproc(n) < 0)
                return -1;
        return addr;	
}

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

// TODO: add support for mmap and munmap syscall.
// hint: read through docstrings in vm.c. Watching CH4 video may also help.
// Note the return value and PTE flags (especially U,X,W,R)
/*
* LAB1: you may need to define sys_trace here
*/
int sys_trace(int trace_request, unsigned long id, uint8 data)
{
	if(trace_request==0){
		uint8 val;
		pte_t *pte = walk(curr_proc()->pagetable,(uint64)id, 0);
		if(pte == 0){
			return -1;
		}
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

extern char trap_page[];

void syscall()
{
	struct trapframe *trapframe = curr_proc()->trapframe;
	int id = trapframe->a7, ret;
	uint64 args[6] = { trapframe->a0, trapframe->a1, trapframe->a2,
			   trapframe->a3, trapframe->a4, trapframe->a5 };
	tracef("syscall %d args = [%x, %x, %x, %x, %x, %x]", id, args[0],
	       args[1], args[2], args[3], args[4], args[5]);
	/*
	* LAB1: you may need to update syscall counter here
	*/
	curr_proc()->syscall_num[id]++;

	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], args[1], args[2]);
		break;
	case SYS_exit:
		sys_exit(args[0]);
		// __builtin_unreachable();
	case SYS_sched_yield:
		ret = sys_sched_yield();
		break;
	case SYS_gettimeofday:
		ret = sys_gettimeofday((TimeVal *)args[0], args[1]);
		break;
	case SYS_sbrk:
		ret = sys_sbrk(args[0]);
		break;
	/*
	* LAB1: you may need to add SYS_trace case here
	*/
	case SYS_trace:
		ret = sys_trace(args[0],args[1],args[2]);
		break;
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
