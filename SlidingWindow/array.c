#include "connect.h"

/* ADT ARRAY */
typedef struct {
    int* data;
    size_t used;
    size_t size;
} Array;

void initArray(Array* a, size_t initialSize) {
    a -> data = (int*) malloc(initialSize * sizeof(int));
    a -> used = 0;
    a -> size = initialSize;
}

void insertArray(Array* a, int element) {
    if (a -> used == a -> size) {
        a -> size *= 2;
        a -> data = (int*) realloc(a -> data, a -> size * sizeof(int));
    }
    a -> data[a -> used++] = element;
}

void freeArray(Array* a) {
    //free(a -> data);
    a -> data = 0;
    a -> used = a -> size = 0;
}

/* ADT ARRAY FRAME */
typedef struct {
    int id;
    int length;
    unsigned char text[FRAMEDATASIZE];
} FrameData;

typedef struct {
    FrameData* data;
    size_t used;
    size_t size;
} ArrayFrame;

void initArrayFrame(ArrayFrame* a, size_t initialSize) {
    a -> data = (FrameData*) malloc(initialSize * sizeof(FrameData));
    a -> used = 0;
    a -> size = initialSize;
}

void insertArrayFrame(ArrayFrame *a, FrameData element) {
    if (a -> used == a -> size) {
        a -> size *= 2;
        a -> data = (FrameData*) realloc(a -> data, a -> size * sizeof(FrameData));
    }
    a -> data[a -> used++] = element;
}

void freeArrayFrame(ArrayFrame* a) {
    //free(a -> data);
    a -> data = 0;
    a -> used = a -> size = 0;
}
