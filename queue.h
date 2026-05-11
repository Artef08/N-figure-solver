#ifndef QUEUE_H
#define QUEUE_H

#include <gtk/gtk.h>
#include <glib.h>

typedef struct Node Node;

struct Node{
    Node* prev;
    Node* next;
    int* solve;
};


typedef struct{
    Node* tail;
    Node* front;
    int size;
} Queue;

Queue* InitQueue(int size);
int QueuePushBack(Queue* q,int* solution);
int QueuePushFront(Queue* q,int* solution);
int QueuePopFront(Queue* q,int** curSolve);
int QueuePopBack(Queue* q,int** curSolve);
void QueueFree(Queue* q);
int* QueuePeekFront(Queue *q); 

#endif
