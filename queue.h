// queue.h
#ifndef QUEUE_H
#define QUEUE_H

#include "playlist.h"

typedef struct QueueNode {
    Track *track;
    struct QueueNode *next;
} QueueNode;

typedef struct Queue {
    QueueNode *front;
    QueueNode *rear;
} Queue;

Queue *queue_create();
void queue_enqueue(Queue *q, Track *track);
Track *queue_dequeue(Queue *q);
int queue_is_empty(Queue *q);
void queue_destroy(Queue *q);

#endif
