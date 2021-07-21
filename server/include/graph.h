#ifndef MY_GRAPH_H
#define MY_GRAPH_H

#include <utils.h>
#include <queue.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

typedef struct _edges {
  void *data; //#nodi adiacenti
  int color;
  queue *adj; // nodi adiacenti
} node;

typedef struct {
    int V;
    queue *E;
} graph;

int graphDetectCycles(graph *g, int (*cmpEdges)(void *, void *), _Bool killFirstCycle);

void freeEdge(void *a);

int graphInsert(graph *g, void *data, queue *edges);

void graphDestroy(graph **g);

int graphAddEdge(graph *g, void *from, void *to, int(*cmpDataNode)(void *, void *));

graph *graphCreate(void (*freeValue)(void *), int (*compare)(void *, void *));


#endif