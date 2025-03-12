# lab4

宋建昊 2022010853

## 功能实现

```c
//增加link_count相关定义，初始化和维护
//string.c
//增加strcmp函数用于文件名比较
int strcmp(const char *str1, const char *str2) {
    while (*str1 && *str2 && *str1 == *str2) {
        str1++;
        str2++;
    }
    return (int)(*str1 - *str2);
}
//sys_spawn函数改动
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
//sys_fstate函数实现
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

//sys_linkat函数实现
//注意使用iput()把每个非空的inode*进行释放
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
//sys_unlinkat函数实现
//注意使用iput()把每个非空的inode*进行释放
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

```

## 问答题

在我们的文件系统中，root inode起着什么作用？如果root inode中的内容损坏了，会发生什么？

root inode 作用为是根目录 inode，搜索需要依赖 root inode 。如果 root inode 的内容损坏，则会导致无法按文件名找到正确的文件，导致各类文件操作无法正常进行。

举出使用 pipe 的一个实际应用的例子。

在 Linux 中，管道（pipe）的一个实际应用是实现 shell 中的命令组合，例如将 ps aux 的输出通过管道传递给 grep root，以筛选包含 "root" 的进程信息：一个进程生成所有进程的列表，另一个进程从中过滤特定模式，管道则负责将前者的输出高效地传递给后者作为输入，常用于数据流处理或日志过滤等场景。

如果需要在多个进程间互相通信，则需要为每一对进程建立一个管道，非常繁琐，请设计一个更易用的多进程通信机制。

可以设计一个基于共享内存环形缓冲区的多进程通信机制：所有进程共享一块内存区域，划分为多个环形缓冲区，每个进程拥有唯一的读写索引，通过原子操作或轻量锁（如自旋锁）协调访问，消息按发送者和接收者标记存入缓冲区，进程根据标记读取属于自己的数据，从而实现多对多的通信，避免为每对进程单独建立管道的繁琐，同时提供高效、低开销的通信方式。

## 荣誉准则

1. 在完成本次实验的过程（含此前学习的过程）中，我曾分别与 **以下各位** 就（与本次实验相关的）以下方面做过交流，还在代码中对应的位置以注释形式记录了具体的交流对象及内容：

   > 无
   >
2. 此外，我也参考了 **以下资料** ，还在代码中对应的位置以注释形式记录了具体的参考来源及内容：

   > 无
   >
3. 我独立完成了本次实验除以上方面之外的所有工作，包括代码与文档。 我清楚地知道，从以上方面获得的信息在一定程度上降低了实验难度，可能会影响起评分。
4. 我从未使用过他人的代码，不管是原封不动地复制，还是经过了某些等价转换。 我未曾也不会向他人（含此后各届同学）复制或公开我的实验代码，我有义务妥善保管好它们。 我提交至本实验的评测系统的代码，均无意于破坏或妨碍任何计算机系统的正常运转。 我清楚地知道，以上情况均为本课程纪律所禁止，若违反，对应的实验成绩将按“-100”分计。