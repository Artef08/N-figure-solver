#include<stdio.h>
#include<stdlib.h>
#include <gtk/gtk.h>
#include <glib.h>
#include "queue.h"

Queue* InitQueue(int size){
    Queue* q=(Queue*)malloc(sizeof(Queue));
    if(!q)return NULL;
    q->front=NULL;
    q->tail=NULL;
    q->size=size;
    return q;  
}

int QueuePushBack(Queue* q,int* solution){
    Node* newNode=(Node*)malloc(sizeof(Node));
    if(!newNode)return -1;
    newNode->solve=g_new(int, q->size);
    memcpy(newNode->solve,solution,q->size*sizeof(int));
    newNode->next=NULL;
    newNode->prev=q->tail;
    if(q->tail)
        q->tail->next=newNode;
    else
        q->front=newNode;
    q->tail=newNode;
    return 0;
}

int QueuePushFront(Queue* q,int* solution){
    Node* newNode=(Node*)malloc(sizeof(Node));
    if(!newNode)return -1;
    newNode->solve=g_new(int, q->size);
    memcpy(newNode->solve,solution,q->size*sizeof(int));
    newNode->prev=NULL;
    newNode->next=q->front;

    if(q->front){
        q->front->prev=newNode;
    }else{
        q->tail=newNode;
    }
    q->front=newNode;
    return 0;
}

int QueuePopFront(Queue* q,int** curSolve){
    if(!q->front)return -1;

    Node* oldFront=q->front;
    *curSolve=oldFront->solve;
    q->front=oldFront->next;
    if(q->front){
        q->front->prev=NULL;
    }else{
        q->tail=NULL;
    }
    free(oldFront);
    return 0;
}

int QueuePopBack(Queue* q,int** curSolve){
    if(!q->tail)return -1;

    Node* oldBack=q->tail;
     *curSolve=oldBack->solve;

    q->tail=oldBack->prev;
    if(q->tail){
        q->tail->next=NULL;
    }else{
        q->front=NULL;
    }
    free(oldBack);
    return 0;
}

void QueueFree(Queue* q){
    Node* iter=q->front;
    while(iter){
        Node* next=iter->next;
        g_free(iter->solve);
        free(iter);
        iter=next;
    }
    free(q);
}

int* QueuePeekFront(Queue *q) {
    return q->front ? q->front->solve : NULL;
}