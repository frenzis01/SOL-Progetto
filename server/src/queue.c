// #include "queue.h"
#include <queue.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define eq_z(s, e, c) \
	if (!(s))         \
	{                 \
		errno = e;    \
		c;            \
		perror(#s);   \
	}

queue *queueCreate(void (*freeValue)(void *), int (*compare)(void *, void *))
{
	queue *q = malloc(sizeof(queue));
	eq_z(q, errno, return NULL);
	q->size = 0;
	q->head = q->tail = NULL;

	q->freeValue = freeValue;
	q->compare = compare;
	return q;
}

/**
 * @returns 0 success, -1 error
 */
int queueEnqueue(queue *q, void *_data)
{
	eq_z(q, EINVAL, return -1;);

	data *toInsert = malloc(sizeof(data));
	eq_z(toInsert, errno, return -1);

	toInsert->next = NULL;
	toInsert->prev = NULL;
	toInsert->data = _data;
	if (q->size == 0)
	{ //First insertion
		q->head = q->tail = toInsert;
	}
	else
	{
		q->tail->next = toInsert;
		toInsert->prev = q->tail;
		q->tail = toInsert;
	}

	q->size++;
	return 0;
}

void *queueDequeue(queue *q)
{
	eq_z(q, EINVAL, return NULL;);

	if (!(q->head))
		return NULL;

	data *toDel = q->head;
	void *toRet = q->head->data;

	if (q->size == 1)
	{
		free(toDel);
		q->head = q->tail = NULL;
		q->size--;
		return toRet;
	}
	q->head->next->prev = NULL;
	q->head = q->head->next;
	free(toDel);
	q->size--;
	return toRet;
}

void *queuePeek(queue *q)
{
	eq_z(q, EINVAL, return NULL;);

	if (!q->head)
		return NULL;
	return q->head->data;
}

void queueClear(queue *q)
{
	eq_z(q, EINVAL, return;);

	while (!queueIsEmpty(q))
	{
		data *temp = q->head;
		q->head = q->head->next;
		q->freeValue(temp->data);
		free(temp);
		q->size--;
	}
}

size_t queueGetSize(queue *q)
{
	eq_z(q, EINVAL, return 0;);

	return q->size;
}

_Bool queueIsEmpty(queue *q)
{
	eq_z(q, EINVAL, return 1;);

	if (q->size == 0)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void *queueFind(queue *q, void *toFind, int (*compare)(void *, void *))
{
	eq_z(q, EINVAL, return NULL;);

	int (*cmpfunc)(void *, void *) = q->compare;
	if (compare)
		cmpfunc = compare;

	data *curr = q->head;

	while (curr && !(cmpfunc(curr->data, toFind)))
		curr = curr->next;

	if (curr)
		return curr->data;
	return NULL;
}

/**
 * Removes a node from the queue and returns it
 */
void *queueRemove(queue *q, void *toRemove, int (*compare)(void *, void *))
{
	errno = 0;
	eq_z(q, EINVAL, return NULL;);

	int (*cmpfunc)(void *, void *) = q->compare;
	if (compare)
		cmpfunc = compare;

	data *curr = q->head;
	while (curr && !(cmpfunc(curr->data, toRemove)))
		curr = curr->next;

	if (!curr)
		return NULL;

	if (!curr->prev)
	{ // curr = head
		q->head = curr->next;
	}
	else
	{
		curr->prev->next = curr->next;
	}
	if (!curr->next)
	{ // curr = tail
		q->tail = curr->prev;
	}
	else
	{
		curr->next->prev = curr->prev;
	}

	void *toRet = curr->data;
	free(curr);
	curr = NULL;
	q->size--;
	return toRet;
}

/**
 * executes 'callback' on every element of the queue.
 * Stops prematurely if errno gets set
 * @param callback must set errno only in case of error (!)
 * @returns 0 success, -1 error
 */
int queueCallback(queue *q, void(callback)(void *))
{
	errno = 0;
	eq_z(q, EINVAL, return 0;);

	data *curr = q->head;
	while (!errno && curr)
	{
		// errno might get dirty here
		callback(curr->data);
		curr = curr->next;
	}
	if (errno) return -1;
	return 0;
}

/**
 * Removes a node from the queue and returns its value
 */
void *queueRemove_node(queue *q, data *toRemove)
{
	errno = 0;
	eq_z(q, EINVAL, return NULL;);

	if (!toRemove->prev)
	{ // toRemove testa
		q->head = toRemove->next;
	}
	else
	{
		toRemove->prev->next = toRemove->next;
	}
	if (!toRemove->next)
	{ // toRemove coda
		q->tail = toRemove->prev;
	}
	else
	{
		toRemove->next->prev = toRemove->prev;
	}

	void *toRet = toRemove->data;
	free(toRemove);
	q->size--;
	return toRet;
}

void queueDestroy(queue *q)
{
	eq_z(q, EINVAL, return;);

	queueClear(q);
	free(q);
}
