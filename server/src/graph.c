/*


Dato un grafo connesso e non orientato, scrivere algoritmo che, dato in input
un vertice r, restituisca il numero di vertici che si trovano a max distanza
da r.


*/
#include <graph.h>

#define NOTVISITED 0
#define ANCESTOR 1
#define VISITED 2

void zeroColor(void *arg)
{
    node *e = arg;
    e->color = 0;
}

int DFSanyCycles(node *curr, int (*cmpEdges)(void *, void *), _Bool removeEdge)
{
    ec_z(curr, return 0); // may happen only on the first call
    curr->color = ANCESTOR;
    data *q_adjNext = curr->adj->head;
    node *adjNext = q_adjNext ? q_adjNext->data : NULL;
    while (adjNext)
    {
        if (adjNext->color == ANCESTOR){
            if (removeEdge) {
                queueRemove_node(curr->adj,q_adjNext);
            }
            return 1;
        }
        if (adjNext->color == NOTVISITED && DFSanyCycles(adjNext, cmpEdges, removeEdge))
            return 1;
        q_adjNext = q_adjNext->next;
        adjNext = q_adjNext ? q_adjNext->data : NULL;
    }
    curr->color = VISITED;
    return 0;
}

int graphDetectCycles(graph *g, int (*cmpEdges)(void *, void *), _Bool killFirstCycle)
{
    ec_z(g,errno = EINVAL; return -1);
    queueCallback(g->E, zeroColor);
    return DFSanyCycles(queuePeek(g->E), cmpEdges, killFirstCycle);
}

queue *wrapQueue(queue *a)
{
    queue *b;
    ec_z(b = queueCreate(freeNothing, NULL), return NULL);
    data *curr = a ? queueDequeue(a) : NULL;
    while (curr)
    {
        queueEnqueue(b, curr->data);
    }
    if (a)
        queueDestroy(a);
    return b;
}

void freeEdge(void *a)
{
    node *e = a;
    queueDestroy(e->adj);
    free(e);
}

int graphInsert(graph *g, void *data, queue *dataAdj)
{
    ec_z(g,errno = EINVAL; return -1);
    node *newEdge;
    ec_z(newEdge = malloc(sizeof(node)), return -1);
    newEdge->data = data;
    ec_z(newEdge->adj = wrapQueue(dataAdj), free(newEdge); return -1);
    ec_neg1(queueEnqueue(g->E, newEdge), queueDestroy(newEdge->adj); free(newEdge); return -1);
    return 0;
}

void graphDestroy(graph **g)
{
    queueDestroy((*g)->E);
    free(*g);
    *g = NULL;
    return;
}

int graphAddEdge(graph *g, void *from, void *to, int (*cmpDataNode)(void *, void *))
{
    ec_z(g,errno = EINVAL; return -1);
    // get the nodes corresponding to 'from' and 'to'
    node *f = NULL,
         *t = NULL;
    // if the nodes don't exist, create them
    queue *toInsertAdj = NULL;
    if (!(f = queueFind(g->E, from, cmpDataNode)))
    {
        ec_z(toInsertAdj = queueCreate(freeNothing, NULL), return -1);
        ec_neg1(graphInsert(g, from, toInsertAdj), queueDestroy(toInsertAdj); return -1);
    }
    if (!(t = queueFind(g->E, to, cmpDataNode)))
    {
        ec_z(toInsertAdj = queueCreate(freeNothing, NULL), return -1);
        ec_neg1(graphInsert(g, to, toInsertAdj), queueDestroy(toInsertAdj); return -1);
    }
    // get the two nodes
    ec_z(f = queueFind(g->E, from, cmpDataNode), return -1);
    ec_z(t = queueFind(g->E, to, cmpDataNode), return -1);
    ec_neg1(queueEnqueue(f->adj, t), return -1);
    return 0;
}

graph *graphCreate(void (*freeValue)(void *), int (*compare)(void *, void *))
{
    graph *g = malloc(sizeof(graph));
    ec_z(g, return NULL);
    ec_z(g->E = queueCreate(freeValue, compare), return NULL);
    return g;
}