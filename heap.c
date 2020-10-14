#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "heap.h"
#include "custom_unistd.h"
#define PAGE_SIZE       4096    // Długość strony w bajtach

heap manager = {.start = 0, .end = 0};

int heap_setup(void){

    void * a = custom_sbrk(PAGE_SIZE);

    if ((intptr_t)a == -1)
        return -1;

    manager.start = (intptr_t)a;
    manager.end = (intptr_t)a + PAGE_SIZE;

    return 0;
}

void heap_clean(void){

    custom_sbrk(-(manager.end - manager.start));
    manager.start = 0;
    manager.end = 0;

}

void* heap_malloc(size_t size){

    return NULL;

}

void* heap_calloc(size_t number, size_t size){

    return NULL;

}

void* heap_realloc(void* memblock, size_t count){

    return NULL;

}

void  heap_free(void* memblock){



}

size_t   heap_get_largest_used_block_size(void){

    return 0;

}

enum pointer_type_t get_pointer_type(const void* const pointer){

    return pointer_null;

}

int heap_validate(void){

    return 0;

}

void* heap_malloc_aligned(size_t count){

    return NULL;

}

void* heap_calloc_aligned(size_t number, size_t size){

    return NULL;

}

void* heap_realloc_aligned(void* memblock, size_t size){

    return NULL;

}

//suma wszystkich bajtów w strukturze == pole z sumą w strukturze, plotek 8 bajtów
