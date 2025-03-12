#include "syscall.h"
#include "console.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"

#define DIR 0x040000              // directory
#define FILE 0x100000             // ordinary regular file

typedef struct Stat {
	uint64 dev;
	uint64 ino;
	uint32 mode;
	uint32 nlink;
	uint64 pad[7];
} Stat;

uint64 console_write(uint64 va, uint64 len)
{
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	int size = copyinstr(p->pagetable, str, va, MIN(len, MAX_STR_LEN));
	tracef("write size = %d", size);
	for (int i = 0; i < size; ++i) {
		console_putchar(str[i]);
	}
	return len;
}

uint64 console_read(uint64 va, uint64 len)
{
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	tracef("read size = %d", len);
	for (int i = 0; i < len; ++i) {
		int c = consgetc();
		str[i] = c;
	}
	copyout(p->pagetable, va, str, len);
	return len;
}

uint64 sys_write(int fd, uint64 va, uint64 len)
{
	if (fd < 0 || fd > FD_BUFFER_SIZE)
		return -1;
	struct proc *p = curr_proc();
	struct file *f = p->files[fd];
	if (f == NULL) {
		errorf("invalid fd %d\n", fd);
		return -1;
	}
	switch (f->type) {
	case FD_STDIO:
		return console_write(va, len);
	case FD_INODE:
		return inodewrite(f, va, len);
	default:
		panic("unknown file type %d\n", f->type);
	}
}

uint64 sys_read(int fd, uint64 va, uint64 len)
{
	if (fd < 0 || fd > FD_BUFFER_SIZE)
		return -1;
	struct proc *p = curr_proc();
	struct file *f = p->files[fd];
	if (f == NULL) {
		errorf("invalid fd %d\n", fd);
		return -1;
	}
	switch (f->type) {
	case FD_STDIO:
		return console_read(va, len);
	case FD_INODE:
		return inoderead(f, va, len);
	default:
		panic("unknown file type %d\n", f->type);
	}
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

uint64 sys_gettimeofday(uint64 val, int _tz)
{
	struct proc *p = curr_proc();
	uint64 cycle = get_cycle();
	TimeVal t;
	t.sec = cycle / CPU_FREQ;
	t.usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	copyout(p->pagetable, val, (char *)&t, sizeof(TimeVal));
	return 0;
}

uint64 sys_getpid()
{
	return curr_proc()->pid;
}

uint64 sys_getppid()
{
	struct proc *p = curr_proc();
	return p->parent == NULL ? IDLE_PID : p->parent->pid;
}

uint64 sys_clone()
{
	debugf("fork!");
	return fork();
}

static inline uint64 fetchaddr(pagetable_t pagetable, uint64 va)
{
	uint64 *addr = (uint64 *)useraddr(pagetable, va);
	return *addr;
}

uint64 sys_exec(uint64 path, uint64 uargv)
{
	struct proc *p = curr_proc();
	char name[MAX_STR_LEN];
	copyinstr(p->pagetable, name, path, MAX_STR_LEN);
	uint64 arg;
	static char strpool[MAX_ARG_NUM][MAX_STR_LEN];
	char *argv[MAX_ARG_NUM];
	int i;
	for (i = 0; uargv && (arg = fetchaddr(p->pagetable, uargv));
	     uargv += sizeof(char *), i++) {
		copyinstr(p->pagetable, (char *)strpool[i], arg, MAX_STR_LEN);
		argv[i] = (char *)strpool[i];
	}
	argv[i] = NULL;
	return exec(name, (char **)argv);
}

uint64 sys_wait(int pid, uint64 va)
{
	struct proc *p = curr_proc();
	int *code = (int *)useraddr(p->pagetable, va);
	return wait(pid, code);
}

uint64 sys_spawn(uint64 va)
{
	// TODO: your job is to complete the sys call
	char program_name[200];
	copyinstr(curr_proc()->pagetable, program_name, va, 200);
	
	struct inode* id=namei(program_name);
	if(id==0){
		return -1;
	}
	struct proc *newproc = allocproc();
	if(newproc== 0){
		return -1;
	}

	bin_loader(id, newproc);
	newproc->parent = curr_proc();
	init_stdio(newproc);
	add_task(newproc);
	iput(id);
	return newproc->pid;
}

uint64 sys_set_priority(long long prio){
    // TODO: your job is to complete the sys call
    if(prio < 2){
		return -1;
	}

	curr_proc()->priority = prio;
	return prio;
}

uint64 sys_openat(uint64 va, uint64 omode, uint64 _flags)
{
	struct proc *p = curr_proc();
	char path[200];
	copyinstr(p->pagetable, path, va, 200);
	return fileopen(path, omode);
}

uint64 sys_close(int fd)
{
	if (fd < 0 || fd > FD_BUFFER_SIZE)
		return -1;
	struct proc *p = curr_proc();
	struct file *f = p->files[fd];
	if (f == NULL) {
		errorf("invalid fd %d", fd);
		return -1;
	}
	fileclose(f);
	p->files[fd] = 0;
	return 0;
}

int sys_fstat(int fd, uint64 stat)
{
	//TODO: your job is to complete the syscall
	struct Stat state;
	if (fd < 0 || fd >= FD_BUFFER_SIZE ) {
		return -1;
	}
	struct file *f = curr_proc()->files[fd];
	if(f == 0||f->type == FD_NONE){
		return -1;
	}
	state.dev = f->ip->dev;
	state.ino = f->ip->inum;
	if(f->ip->type == T_DIR){
		state.mode = DIR;
	}else{
		state.mode = FILE;
	}

	state.nlink = f->ip->link_count;
	int copy = copyout(curr_proc()->pagetable, stat, (char *)&state, sizeof(struct Stat));
	if(copy == -1){
		return -1;
	}
	return 0;
}

int sys_linkat(int olddirfd, uint64 oldpath, int newdirfd, uint64 newpath,
	       uint64 flags)
{
	//TODO: your job is to complete the syscall
	if(oldpath == 0 || newpath == 0){
		return -1;
	}
	char old_path[DIRSIZ];
	char new_path[DIRSIZ];
	int old = copyinstr(curr_proc()->pagetable, old_path, oldpath, DIRSIZ);
	int new = copyinstr(curr_proc()->pagetable, new_path, newpath, DIRSIZ);
	if(old == -1 || new == -1){
		return -1;
	}
	if(strcmp(old_path, new_path) == 0){
		return -1;
	}
	struct inode* ip,*old_file,*new_file;
	ip = root_dir();
	old_file = dirlookup(ip, old_path, 0);
	if(old_file == 0){
		iput(ip);
		return -1;
	}
	ivalid(old_file);
	new_file = dirlookup(ip, new_path, 0);
	if(new_file != 0){
		iput(new_file);
		iput(ip);
		iput(old_file);
		return -1;
	}
	if(dirlink(ip, new_path, old_file->inum) < 0){
		iput(ip);
		iput(old_file);
		return -1;
	}
	old_file->link_count++;
	iupdate(old_file);
	iput(ip);
	iput(old_file);
	return 0;
}

int sys_unlinkat(int dirfd, uint64 name, uint64 flags)
{
	//TODO: your job is to complete the syscall
	char filename[DIRSIZ];
	int copy = copyinstr(curr_proc()->pagetable, filename, name, DIRSIZ);
	if(copy == -1){
		return -1;
	}
	struct inode* ip,*file;
	ip = root_dir();
	file = dirlookup(ip, filename, 0);
	if(file == 0){
		iput(ip);
		return -1;
	}
	ivalid(file);
	if(dirunlink(ip, filename) < 0){
		iput(ip);
		iput(file);
		return -1;
	}
	file->link_count--;
	iupdate(file);
	iput(file);
	iput(ip);
	return 0;
}

uint64 sys_sbrk(int n)
{
	uint64 addr;
	struct proc *p = curr_proc();
	addr = p->program_brk;
	if (growproc(n) < 0)
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

extern char trap_page[];

void syscall()
{
	struct trapframe *trapframe = curr_proc()->trapframe;
	int id = trapframe->a7, ret;
	uint64 args[6] = { trapframe->a0, trapframe->a1, trapframe->a2,
			   trapframe->a3, trapframe->a4, trapframe->a5 };
	tracef("syscall %d args = [%x, %x, %x, %x, %x, %x]", id, args[0],
	       args[1], args[2], args[3], args[4], args[5]);
	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], args[1], args[2]);
		break;
	case SYS_read:
		ret = sys_read(args[0], args[1], args[2]);
		break;
	case SYS_openat:
		ret = sys_openat(args[0], args[1], args[2]);
		break;
	case SYS_close:
		ret = sys_close(args[0]);
		break;
	case SYS_exit:
		sys_exit(args[0]);
		// __builtin_unreachable();
	case SYS_sched_yield:
		ret = sys_sched_yield();
		break;
	case SYS_gettimeofday:
		ret = sys_gettimeofday(args[0], args[1]);
		break;
	case SYS_getpid:
		ret = sys_getpid();
		break;
	case SYS_getppid:
		ret = sys_getppid();
		break;
	case SYS_clone: // SYS_fork
		ret = sys_clone();
		break;
	case SYS_execve:
		ret = sys_exec(args[0], args[1]);
		break;
	case SYS_wait4:
		ret = sys_wait(args[0], args[1]);
		break;
	case SYS_fstat:
		ret = sys_fstat(args[0], args[1]);
		break;
	case SYS_linkat:
		ret = sys_linkat(args[0], args[1], args[2], args[3], args[4]);
		break;
	case SYS_unlinkat:
		ret = sys_unlinkat(args[0], args[1], args[2]);
		break;
	case SYS_spawn:
		ret = sys_spawn(args[0]);
		break;
	case SYS_sbrk:
		ret = sys_sbrk(args[0]);
		break;
	/*
	* LAB1: you may need to add SYS_trace case here
	*/

	case SYS_mmap:
		ret = sys_mmap((void*)args[0], args[1], args[2],
			       args[3]);
		break;
	case SYS_munmap:
		ret = sys_munmap((void*)args[0], args[1]);
		break;
	case SYS_setpriority:
		ret = sys_set_priority((long long)args[0]);
		break;
	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}
	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}
