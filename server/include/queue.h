#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct _data {
	void * data;
	struct _data * next;
	struct _data * prev;
}data;

typedef struct _queue {
	size_t size;
	size_t allocationSize;
	void (*freeValue)(void*);
	int (*compare)(void*,void*);
	data* head;
	data* tail;
}queue;
/*
Create and return an empty queue
*/
queue* queueCreate(/*size_t allocSize,*/void (*freeValue)(void*),int (*compare)(void*,void*), int *E_QUEUE);
/*
Insert data into the queue(last position)
*/
void queueEnqueue(queue * q, void* data, int *E_QUEUE);
/*
Remove first element of the queue of save its value to the toRet argument
*/
void * queueDequeue(queue * q, int *E_QUEUE);
/*
Save first element of the queue to the toRet argument
*/
void *queuePeek(queue*q, int *E_QUEUE);//Return the first element
/*
Deletes all data of the queue
*/
void queueClear(queue* q, int *E_QUEUE);
/*
Clears and destoys the queue
*/
void queueDestroy(queue *q, int *E_QUEUE);
/*
Return size of the queue
*/
size_t queueGetSize(queue *q, int *E_QUEUE);
/*
Check is queue is empty
*/
bool queueIsEmpty(queue * q, int *E_QUEUE);

void *queueFind(queue *q, void *toFind, int (*compare)(void *, void *), int *E_QUEUE);

void *queueRemove(queue *q, void *toRemove, int (*compare)(void *, void *), int *E_QUEUE);

void queueCallback(queue *q, void (callback)(void *));

void *queueRemove_node(queue *q, data *toRemove, int *E_QUEUE);

#endif
