#ifndef MY_GRAPH_H
#define MY_GRAPH_H

#include <utils.h>
#include <queue.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct _edges {
  void *data; //#nodi adiacenti
  int color;
  queue *adj; // nodi adiacenti
} node;

typedef struct {
    int V;
    queue *E;
} graph;

int graphInsert(graph *g, void *data, queue *edges);

#endif