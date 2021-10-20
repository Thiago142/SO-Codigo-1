#include <stdio.h>
#include <stdlib.h>
#include "fifo.h"


void queue_create(FIFO *fi) {
    fi->front = 0;
    fi->pos = MAXSIZE -1;
    fi->size = 0;
}

int queue_push(FIFO *fi, int value) {//enqueue
    if (queue_isfull(fi))
        return 1;//está cheia

    fi->pos = (fi->pos + 1) % MAXSIZE;
    fi->rand_numbers[fi->pos] = value;
    fi->size++;

    return 2;//está vazia 
}

int queue_getfirst(FIFO *fi) {
    if (queue_isempty(fi))
        return -2;

    int val = fi->rand_numbers[fi->front];
    return val;
}

int queue_pop(FIFO *fi) {
    if (queue_isempty(fi))
        return -2;

    fi->front = (fi->front + 1) % MAXSIZE;
    fi->size--;
}

int queue_isfull(FIFO *fi) {
    return fi->size == MAXSIZE;
}

int queue_isempty(FIFO *fi) {
    return fi->size == 0;
}

int queue_size(FIFO *fi){
    return fi->size;
}