#include "connect.h"

/* ===== ADT ARRAY ===== */
typedef struct {
    unsigned char* data;
    size_t used;
    size_t size;
} Array;

void initArray(Array* a) {
    int initialSize = 1;
    a -> data = (unsigned char*) malloc(initialSize * sizeof(unsigned char));
    a -> used = 0;
    a -> size = initialSize;
}

void insertArray(Array* a, int element) {
    if (a -> used == a -> size) {
        a -> size *= 2;
        a -> data = (unsigned char*) realloc(a -> data, a -> size * sizeof(unsigned char));
        if ((a -> data) == NULL) {
            printf("realloc failed!\n");
        }
    }
    a -> data[a -> used++] = element;
}

/* ===== ADT ARRAY FRAME ===== */
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

void initArrayFrame(ArrayFrame* a) {
    int initialSize = 1;
    a -> data = (FrameData*) malloc(initialSize * sizeof(FrameData));
    a -> used = 0;
    a -> size = initialSize;
}

void insertArrayFrame(ArrayFrame* a, FrameData element) {
    if (a -> used == a -> size) {
        a -> size *= 2;
        a -> data = (FrameData*) realloc(a -> data, a -> size * sizeof(FrameData));
    }
    a -> used++;
    a -> data[element.id] = element;
}
