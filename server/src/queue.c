// #include "queue.h"
#include <queue.h>
#include <string.h>
#include <stdio.h>

#define eq_z(s, x, c) \
	if (!(s))         \
	{                 \
		*E_QUEUE = x; \
		c;            \
		perror(#s);   \
	}

// int *E_QUEUE = 0;
#define E_QUEUE_MALLOC 1
#define E_QUEUE_INVARG 2

// allocSize inutile...
queue *queueCreate( void (*freeValue)(void *), int (*compare)(void *, void *), int *E_QUEUE)
{
	queue *q = malloc(sizeof(queue));
	eq_z(q, E_QUEUE_MALLOC, { return NULL; });
	// q->allocationSize = allocSize;
	q->size = 0;
	q->head = q->tail = NULL;

	q->freeValue = freeValue;
	q->compare = compare;
	return q;
}

void queueEnqueue(queue *q, void *_data, int *E_QUEUE)
{
	eq_z(q, E_QUEUE_INVARG, { return; });

	data *toInsert = (data *)malloc(sizeof(data));
	eq_z(toInsert, E_QUEUE_MALLOC, { return; });

	// Invece di copiare il contenuto di data, assegnamo il void*
	// 	toInsert->data = malloc(q->allocationSize);
	// 	eq_z(toInsert->data, E_QUEUE_MALLOC, { return NULL; });

	toInsert->next = NULL;
	toInsert->prev = NULL;
	toInsert->data = _data;
	// 	memcpy(toInsert->data, _data, q->allocationSize);
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
}

void *queueDequeue(queue *q, int *E_QUEUE)
{

	eq_z(q, E_QUEUE_INVARG, { return NULL; });

	if (!(q->head))
		return NULL;

	data *toDel = q->head;
	void *toRet = q->head->data;

	if (q->size == 1)
	{
		// memcpy(toRet, toDel->data, q->allocationSize);
		// q->freeValue(toDel->data);
		// // free(toDel->data)
		free(toDel);
		q->head = q->tail = NULL;
		q->size--;
		return toRet;
	}
	q->head->next->prev = NULL;
	q->head = q->head->next;
	// memcpy(toRet, toDel->data, q->allocationSize);
	// free(toDel->data);
	free(toDel);
	q->size--;
	return toRet;
}

void *queuePeek(queue *q, int *E_QUEUE)
{
	eq_z(q, E_QUEUE_INVARG, { return NULL; });

	if (!q->head) return NULL;
	return q->head->data;
	// memcpy(toRet, q->head->data, q->allocationSize);
}

void queueClear(queue *q, int *E_QUEUE)
{
	eq_z(q, E_QUEUE_INVARG, { return; });

	while (!queueIsEmpty(q, E_QUEUE))
	{
		data *temp = q->head;
		q->head = q->head->next;
		// free(temp->data);
		q->freeValue(temp->data);
		free(temp);
		q->size--;
	}
}

size_t queueGetSize(queue *q, int *E_QUEUE)
{
	eq_z(q, E_QUEUE_INVARG, { return 0; });

	return q->size;
}

_Bool queueIsEmpty(queue *q, int *E_QUEUE)
{
	eq_z(q, E_QUEUE_INVARG, { return 1; });

	if (q->size == 0)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void *queueFind(queue *q, void *toFind, int (*compare)(void *, void *), int *E_QUEUE)
{
	eq_z(q, E_QUEUE_INVARG, { return NULL; });

	int (*cmpfunc)(void *, void *);
	if (compare == NULL)
		cmpfunc = q->compare;
	else
		cmpfunc = compare;

	data *curr = q->head;
	while (curr && !(cmpfunc(curr->data, toFind)))
		curr = curr->next;

	if (curr)
		return curr->data;
	return NULL;
}

void *queueRemove(queue *q, void *toRemove, int (*compare)(void *, void *), int *E_QUEUE)
{

	eq_z(q, E_QUEUE_INVARG, { return NULL; });

	int (*cmpfunc)(void *, void *) = q->compare;
	if (compare)
		cmpfunc = compare;


	data *curr = q->head;
	while (curr && !(cmpfunc(curr->data, toRemove)))
		curr = curr->next;

	if (!curr)
		return NULL; // se non c'è nella lista, fine
	// altrimenti aggiustiamo i pointer a prec e next

	if (!curr->prev)
	{ // curr testa
		q->head = curr->next;
	}
	else
	{
		curr->prev->next = curr->next;
	}
	if (!curr->next)
	{ // curr coda
		q->tail = curr->prev;
	}
	else
	{
		curr->next->prev = curr->prev;
	}

	void *toRet = curr->data;
	//q->freeValue(curr);
	free(curr);
	curr = NULL;
	q->size--;
	return toRet;
}

void queueCallback(queue *q, void (callback)(void *)) {
	data  *curr= q->head;
	while (curr){
		callback(curr->data);
		curr = curr->next;
	}
}

void *queueRemove_node(queue *q, data *toRemove, int *E_QUEUE)
{
	eq_z(q, E_QUEUE_INVARG, { return NULL; });

	eq_z(toRemove, E_QUEUE_INVARG, { return NULL; });

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

void queueDestroy(queue *q, int *E_QUEUE)
{
	eq_z(q, E_QUEUE_INVARG, { return; });

	queueClear(q, E_QUEUE);
	free(q);
}
