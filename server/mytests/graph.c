#include <graph.h>
#include <utils.h>
#include <conn.h>

#define NCLIENTS 4
#define NFILES 10
#define FDSTART 20

void initClients(Client **fakeClients);
int printClientGraph(graph *g);
int cmpClient(void *a, void *b);
int cmpClientNode(void *a, void *b);
void freeArr(void **arr, size_t n, void (*myfree)(void *arg));

int main(void)
{
    Client *clients[NCLIENTS];
    initClients(clients);
    graph *waitfor = graphCreate(freeEdge, cmpClient);
    graphAddEdge(waitfor, clients[0], clients[1], cmpClientNode);
    graphAddEdge(waitfor, clients[1], clients[2], cmpClientNode);
    graphAddEdge(waitfor, clients[2], clients[3], cmpClientNode);
    graphAddEdge(waitfor, clients[2], clients[0], cmpClientNode);
    printClientGraph(waitfor);
    printf("Deadlock? %s\n", graphDetectCycles(waitfor,cmpClient) ? "Yes" : "No");
    graphDestroy(&waitfor);
    freeArr((void **)clients, NCLIENTS, NULL);
    return 0;
}

void initClients(Client **fakeClients)
{
    puts("InitClients");
    for (size_t i = 0; i < NCLIENTS; i++)
    {
        assert(fakeClients[i] = malloc(sizeof(Client)));
        fakeClients[i]->fd = FDSTART + i;
    }
}

void printClient(void *a)
{
    Client *c = ((node*)a)->data;
    printf("%d ", c->fd);
    return;
}

void printClientNode(void *a)
{
    node *c = a;
    printf("%d adjacent to: ", ((Client *)c->data)->fd);
    queueCallback(c->adj, printClient);
    puts("");
}

int printClientGraph(graph *g)
{
    queueCallback(g->E, printClientNode);
    return 0;
}

int cmpClientNode(void *v, void *a)
{
    node *e = v;
    Client *c = a, *b = e->data;

    if (c->fd == b->fd)
        return 1;
    return 0;
}

int cmpClient(void *a, void *b)
{
    Client *c1 = a, *c2 = b;
    if (c1->fd == c2->fd)
        return 1;
    return 0;
}

void freeArr(void **arr, size_t n, void (*myfree)(void *arg))
{
    for (size_t i = 0; i < n; i++)
    {
        if (arr[i])
            myfree ? myfree(arr[i]) : free(arr[i]);
    }
}
