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

int DFSanyCycles(node *curr, _Bool removeEdge)
{
    ec_z(curr, return 0); // may happen only on the first call
    curr->color = ANCESTOR;
    data *q_adjNext = curr->adj->head;
    node *adjNext = q_adjNext ? q_adjNext->data : NULL;
    while (adjNext)
    {
        if (adjNext->color == ANCESTOR)
        {
            if (removeEdge)
            {
                queueRemove_node(curr->adj, q_adjNext);
            }
            return 1;
        }
        if (adjNext->color == NOTVISITED && DFSanyCycles(adjNext, removeEdge))
            return 1;
        q_adjNext = q_adjNext->next;
        adjNext = q_adjNext ? q_adjNext->data : NULL;
    }
    curr->color = VISITED;
    return 0;
}

int graphDetectCycles(graph *g, _Bool killFirstCycle)
{
    ec_z(g, errno = EINVAL; return -1);
    queueCallback(g->E, zeroColor);
    return DFSanyCycles(queuePeek(g->E), killFirstCycle);
}

queue *wrapQueue(queue *a)
{
    queue *b;
    ec_z(b = queueCreate(freeNothing, NULL), return NULL);
    data *curr = a ? queueDequeue(a) : NULL;
    while (curr)
    {
        queueEnqueue(b, curr->data);
        curr = queueDequeue(a);
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
    ec_z(g, errno = EINVAL; return -1);
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

node *wrapNode(void *data)
{
    node *v = malloc(sizeof(node));
    ec_z(v, return NULL);
    v->data = data;
    return v;
}

int graphAddEdge(graph *g, void *from, void *to)
{
    if (!g || !from || !to)
    {
        errno = EINVAL;
        return -1;
    }
    // get the nodes corresponding to 'from' and 'to'
    node *f = NULL,
         *t = NULL,
         *_from = NULL,
         *_to = NULL;

    ec_z(_from = wrapNode(from), return -1);
    ec_z(_to = wrapNode(to), free(_from); return -1);

    // if the nodes don't exist, create them
    queue *toInsertAdj = NULL;
    if (!(f = queueFind(g->E, _from, NULL)))
    {
        ec_z(toInsertAdj = queueCreate(freeNothing, NULL), return -1);
        ec_neg1(graphInsert(g, from, toInsertAdj), queueDestroy(toInsertAdj); return -1);
    }
    if (!(t = queueFind(g->E, _to, NULL)))
    {
        ec_z(toInsertAdj = queueCreate(freeNothing, NULL), return -1);
        ec_neg1(graphInsert(g, to, toInsertAdj), queueDestroy(toInsertAdj); return -1);
    }
    // get the two nodes
    ec_z(f = queueFind(g->E, _from, NULL), return -1);
    ec_z(t = queueFind(g->E, _to, NULL), return -1);
    // add edge
    ec_neg1(queueEnqueue(f->adj, t), return -1);
    free(_from);
    free(_to);

    return 0;
}

int DFSremoveNode(graph *g, node *curr, node *toRemove)
{
    curr->color = ANCESTOR;
    data *q_adjNext = curr->adj->head, *nextAdj = NULL;
    node *adjNext = q_adjNext ? q_adjNext->data : NULL;
    while (adjNext)
    {
        nextAdj = NULL;
        if (g->cmp(adjNext, toRemove))
        {
            nextAdj = q_adjNext->next;
            queueRemove_node(curr->adj, q_adjNext);
            q_adjNext = NULL;
        }
        else if (adjNext->color == NOTVISITED)
            DFSremoveNode(g, adjNext, toRemove);

        q_adjNext = nextAdj ? nextAdj : (q_adjNext ? q_adjNext->next : NULL);
        adjNext = q_adjNext ? q_adjNext->data : NULL;
    }
    curr->color = VISITED;
    return 0;
}

int graphRemoveNode(graph *g, void *toRemove)
{
    if (!toRemove || !g)
    {
        errno = EINVAL;
        return 0;
    }

    ec_neg1(queueCallback(g->E, zeroColor), return -1);

    // have to do some tricks because g->cmp compares two nodes
    // fake node to call queuefind
    node *tmp = wrapNode(toRemove);
    ec_z(tmp, return -1);
    // node *toRem = queueFind(g,tmp,g->cmp); NULL node
    // mb toRem not necessary

    void *_tmp = NULL;
    if (g->E->head)
        DFSremoveNode(g, g->E->head->data, tmp);
    _tmp = queueRemove(g->E, tmp, g->cmp);
    free(tmp);
    if (_tmp) freeEdge(_tmp);
    return 0;
}

int graphRemoveEdge(graph *g, void *from, void *to)
{
    ec_z(g, errno = EINVAL; return -1);
    // get the nodes corresponding to 'from' and 'to'
    node *f = NULL,
         *_from = NULL,
         *_to = NULL;

    ec_z(_from = wrapNode(from), return -1);
    ec_z(_to = wrapNode(to), free(_from); return -1);

    // if the nodes don't exist, create them
    queue *toInsertAdj = NULL;
    if (!(f = queueFind(g->E, _from, NULL)))
    {
        ec_z(toInsertAdj = queueCreate(freeNothing, NULL), goto cleanup);
        ec_neg1(graphInsert(g, from, toInsertAdj), queueDestroy(toInsertAdj); goto cleanup);
        f = queueFind(g->E, _from, NULL);
    }

    queueRemove(f->adj, _to, g->cmp);

    free(_from);
    free(_to);
    return 0;
cleanup:
    free(_from);
    free(_to);
    return -1;
}

graph *graphCreate(void (*freeValue)(void *), int (*compare)(void *, void *))
{
    ec_z(compare, return NULL);
    graph *g = malloc(sizeof(graph));
    ec_z(g, return NULL);
    ec_z(g->E = queueCreate(freeValue, compare), free(g); return NULL);
    g->cmp = compare;
    return g;
}

/**
 * @brief find all nodes that are pointing to f and redirect them to q,
          without removing the old edge
 * 
 * @param arg 
 * @param old 
 * @param new 
 * @param cmp 
 */
void redirect(void *arg, void *old, void *new, void *cmp){
  node *n = arg;
  // queueRemove(n->adj,old,cmp);
  int (*compare)(void*,void*) = cmp; // TODO bad warning
  if (!compare(arg,new) && queueFind(n->adj,old,cmp)){
      if(!queueFind(n->adj,new,cmp))
          queueEnqueue(n->adj, new);  
  }
}

/**
 * @brief if a node points to 'from', adds an edge pointing to 'to', but doesn't remove 'from'
 * 
 * @param g 
 * @param from 'old' value to which other nodes are pointing
 * @param to 'new' to which other nodes will point instead of 'from'
 * @return int 
 */
int graphRedirect(graph *g, void *from, void *to){
    ec_z(g, errno = EINVAL; return -1);
    // get the nodes corresponding to 'from' and 'to'
    node *f = NULL,
         *_from = NULL,
         *_to = NULL,
         *t = NULL;

    ec_z(_from = wrapNode(from), return -1);
    ec_z(_to = wrapNode(to), free(_from); return -1);

    if ((f = queueFind(g->E, _from, NULL))){
        // add 'to' if it doesn't exist
        if (! (t = queueFind(g->E, _to, NULL))) {
            queue *toInsertAdj = NULL;
            ec_z(toInsertAdj = queueCreate(freeNothing, NULL), goto cleanup);
            ec_neg1(graphInsert(g, to, toInsertAdj), queueDestroy(toInsertAdj); goto cleanup);
            t = queueFind(g->E, _to, NULL);
        }
        // find all nodes that are pointing to f and redirect them to q,
        // without removing the old edge
        queueCallbackParam(g->E,redirect,f,t,g->cmp);

    }
    // else // nothing to do

    free(_from);
    free(_to);
    return 0;
    cleanup:
    free(_from);
    free(_to);
    return -1;
}