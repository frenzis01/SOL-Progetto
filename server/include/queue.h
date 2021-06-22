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
queue* queueCreate(/*size_t allocSize,*/void (*freeValue)(void*),int (*compare)(void*,void*));
/*
Insert data into the queue(last position)
*/
void queueEnqueue(queue * q, void* data);
/*
Remove first element of the queue of save its value to the toRet argument
*/
void * queueDequeue(queue * q);
/*
Save first element of the queue to the toRet argument
*/
void *queuePeek(queue*q);//Return the first element
/*
Deletes all data of the queue
*/
void queueClear(queue* q);
/*
Clears and destoys the queue
*/
void queueDestroy(queue *q);
/*
Return size of the queue
*/
size_t queueGetSize(queue *q);
/*
Check is queue is empty
*/
bool queueIsEmpty(queue * q);

void *queueFind(queue *q, void *toFind, int (*compare)(void *, void *));

void *queueRemove(queue *q, void *toRemove, int (*compare)(void *, void *));

void queueCallback(queue *q, void (callback)(void *));

void *queueRemove_node(queue *q, data *toRemove);

#endif
