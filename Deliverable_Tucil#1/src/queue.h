#ifndef QUEUE_H
#define QUEUE_H
 
typedef char *infotype;
 
typedef struct qnode
{
        infotype elem;
        struct qnode *next;
        struct qnode *prev;
}qnode;
 
typedef struct
{
        qnode *back;
        qnode *front;
}queue;
 
/*
 * Create an queue.
 */
queue *q_create(void);
 
/*
 * Destroy an queue.
 * Returns 1 on success, and otherwise 0.
 */
int q_destroy(queue *q);
 
/*
 * Check if queue is empty.
 * Returns 1 if the queue is empty, and otherwise 0.
 */
int q_isempty(queue *q);
 
/*
 * Add element to queue.
 * Returns 1 on success, and otherwise 0.
 */
int q_enqueue(queue *q, infotype elem);
 
/*
 * Remove element from queue, and return it.
 */
infotype q_dequeue(queue *q);
 
#endif