// stack.c
#include "stack.h"
#include <stdlib.h>

Stack *stack_create() {
    Stack *s = malloc(sizeof(Stack));
    s->top = NULL;
    return s;
}

void stack_push(Stack *s, Track *track) {
    if (!s || !track) return;
    StackNode *node = malloc(sizeof(StackNode));
    node->track = track;
    node->next = s->top;
    s->top = node;
}

Track *stack_pop(Stack *s) {
    if (!s || !s->top) return NULL;
    StackNode *node = s->top;
    Track *t = node->track;
    s->top = node->next;
    free(node);
    return t;
}

int stack_is_empty(Stack *s) {
    return (s->top == NULL);
}

void stack_destroy(Stack *s) {
    if (!s) return;
    while (!stack_is_empty(s)) stack_pop(s);
    free(s);
}
