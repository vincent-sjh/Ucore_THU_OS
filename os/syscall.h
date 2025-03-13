#ifndef SYSCALL_H
#define SYSCALL_H

void syscall();
int deadlock_detect(const int available[LOCK_POOL_SIZE],
	const int allocation[NTHREAD][LOCK_POOL_SIZE],
	const int request[NTHREAD][LOCK_POOL_SIZE]);
#endif // SYSCALL_H
