#include "queue.h"
#include "defs.h"
#include "proc.h"

void init_queue(struct queue *q)
{
	q->front = q->tail = 0;
	q->empty = 1;
}


void push_queue(struct queue *q, int value)
{
	if (!q->empty && q->front == q->tail) {
		panic("queue shouldn't be overflow");
	}
	q->empty = 0;
	q->data[q->tail] = value;
	q->tail = (q->tail + 1) % NPROC;
}

int pop_queue(struct queue *q)
{
	if (q->empty)
		return -1;
	long long prior = q->front;
	long long stride = pool[q->data[q->front]].stride;

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
