// stack.h
#ifndef STACK_H
#define STACK_H

#include "playlist.h"

typedef struct StackNode {
    Track *track;
    struct StackNode *next;
} StackNode;

typedef struct Stack {
    StackNode *top;
} Stack;

Stack *stack_create();
void stack_push(Stack *s, Track *track);
Track *stack_pop(Stack *s);
int stack_is_empty(Stack *s);
void stack_destroy(Stack *s);

#endif
