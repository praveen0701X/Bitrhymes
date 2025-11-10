// queue.c
#include "queue.h"
#include <stdlib.h>

Queue *queue_create() {
    Queue *q = malloc(sizeof(Queue));
    q->front = q->rear = NULL;
    return q;
}

void queue_enqueue(Queue *q, Track *track) {
    if (!q || !track) return;
    QueueNode *node = malloc(sizeof(QueueNode));
    node->track = track;
    node->next = NULL;
    if (!q->rear) {
        q->front = q->rear = node;
    } else {
        q->rear->next = node;
        q->rear = node;
    }
}

Track *queue_dequeue(Queue *q) {
    if (!q || !q->front) return NULL;
    QueueNode *node = q->front;
    Track *t = node->track;
    q->front = node->next;
    if (!q->front) q->rear = NULL;
    free(node);
    return t;
}

int queue_is_empty(Queue *q) {
    return (q->front == NULL);
}

void queue_destroy(Queue *q) {
    if (!q) return;
    while (!queue_is_empty(q)) queue_dequeue(q);
    free(q);
}
