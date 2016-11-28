#include <stdlib.h>
#include "queue.h"
 
queue *q_create(void)
{
        queue *q = malloc(sizeof(*q));
        if (!q)
                return NULL;
        q->back = NULL;
        q->front = NULL;
        return q;
}
 
int q_destroy(queue *q)
{
        if (!q)
                return 0;
        while (!q_isempty(q))
                q_dequeue(q);
        free(q);
        return 1;
}
 
int q_isempty(queue *q)
{
        if (!q)
                return 1;
        if (!q->front)
                return 1;
        else
                return 0;
}
 
int q_enqueue(queue *q, infotype elem)
{
        qnode *new = malloc(sizeof(*new));
        if (!new)
                return 0;
        if (!q || !elem)
        {
                free(new);
                return 0;
        }
        new->elem = elem;
        new->next = q->back;
        new->prev = NULL;
        if (q->back)
                q->back->prev = new;
        q->back = new;
        if (!q->front)
                q->front = new;
        return 1;
}
 
infotype q_dequeue(queue *q)
{
        qnode *prev;
        infotype *elem;
        if (q_isempty(q))
                return NULL;
        prev = q->front->prev;
        *elem = q->front->elem;
        free(q->front);
        q->front = prev;
        return *elem;
}