#ifndef HEAP_H
#define HEAP_H

#include <stdlib.h>
#include "model.h"


typedef struct {
    int doc_id;
    double key;
    IV* lp;
}db;

typedef db* node;

typedef struct minHeap {
    int size ;
    node *elem ;
} minHeap ;

minHeap* initMinHeap(int size);
void insertNode(minHeap *hp, node nd);
node deleteNode(minHeap *hp);
void deleteMinHeap(minHeap *hp);

#endif