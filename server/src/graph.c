/*


Dato un grafo connesso e non orientato, scrivere algoritmo che, dato in input
un vertice r, restituisca il numero di vertici che si trovano a max distanza
da r.


*/
#include <graph.h>

#define ANCESTOR 1
#define VISITED 2
#define NOTVISITED 0

void zeroColor(void *arg){
    node *e=arg;
    e->color = 0;
}

int anyCycles(graph *g, int (*cmpEdges)(void *, void *)) {
    queueCallback(g->E, zeroColor);
    return DFSanyCycles(queueDequeue(g->E),cmpEdges);
}

int DFSanyCycles(node *curr, int(*cmpEdges)(void *, void*)) {
    ec_z(curr, return 0);   // may happen only on the first call
    curr->color = ANCESTOR;
    node *adjNext = queueDequeue(curr->adj);
    while (adjNext){
        if (adjNext->color == ANCESTOR)
            return 1; // caller will destroy ancestors
        if (adjNext->color == NOTVISITED)
            return DFSanyCycles(adjNext,cmpEdges);
    }
    adjNext->color == VISITED;
    return 0;
}

queue *wrapQueue(queue *a){
    queue *b;
    ec_neg1(b = queueCreate(NULL,NULL), return NULL);
    data *curr = queueDequeue(a);
    while (curr){
        queueEnqueue(b,curr->data);
    }
    return b;
}

void freeEdge(node **e) {
    queueDestroy((*e)->adj);
    free(*e);
    *e = NULL;
}

int graphInsert(graph *g, void *data, queue *dataAdj)
{
    node *newEdge;
    ec_z(newEdge = malloc(sizeof(node)), return -1);
    newEdge->data = data;
    ec_z(newEdge->adj = wrapQueue(dataAdj),free(newEdge); return -1);
    ec_neg1(queueEnqueue(g->E, newEdge), free(newEdge); return -1);
    return 0;
}

void graphDestroy(graph **g) {
    node *curr = queueDequeue((*g)->E);
    while(curr)
        freeEdge(&curr);
    return;
}

int graphAddEdge(graph *g, void *from, void *to, int(*cmpDataNode)(void *, void *)) {
    // get the nodes corresponding to 'from' and 'to'
    node *f = NULL,
        *to = NULL;
    // if the nodes don't exist, create them
    queue *toInsertAdj = NULL;
    node *toInsert = NULL;
    if (!(f = queueFind(g->E, from, cmpDataNode))) {
        ec_z(toInsertAdj = queueCreate(NULL,NULL), return -1);
        ec_neg1(graphInsert(g,from,toInsertAdj), return -1);
    }
    if (!(g = queueFind(g->E, to, cmpDataNode))) {
        ec_z(toInsertAdj = queueCreate(NULL,NULL), return -1);
        ec_neg1(graphInsert(g,to,toInsertAdj), return -1);
    }
    // get the two nodes
    ec_z(f = queueFind(g->E,from,cmpDataNode), return -1);
    ec_z(g = queueFind(g->E,to,cmpDataNode), return -1);
    ec_neg1(queueEnqueue(f->adj,g), return -1);
    return 0;
}