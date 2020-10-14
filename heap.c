#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "heap.h"
#include "custom_unistd.h"
#define PAGE_SIZE 4096 // page length in bytes
#define FENCE 8 // fence length in bytes
#define CHUNK 32    //struct chunk size
#define CHUNK_REAL 30 //real chunk length
#define CHUNK_CHECK 28 // length of memory to chunk truct sumcheck
#define ADD_SIZE (2 * FENCE + CHUNK)  // addictional allocation length
#define BEF_DATA (FENCE + CHUNK) // from pointer to first data index
#define FENCE_VAL 0xFFFFFFFFFFFFFFFF // value for fence validation

heap manager = {.start = 0, .end = 0, .head = NULL};

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
    manager.head = NULL;

}

unsigned short checksum(void * str){

    if (str == NULL)
        return -1;

    unsigned short a = 0;

    for(int x = 0; x < CHUNK_CHECK; x++){

        a += ((char*)str)[x];
    }

    return a;
}

void* heap_malloc(size_t size){     // can be a problem with assigning pointers (different types)

    if (manager.start == manager.end || size <= 0 || heap_validate())
        return NULL;

    if (manager.head == NULL){

        if (manager.start + size + ADD_SIZE >= manager.end){
            
            if (-1 == custom_sbrk((size + ADD_SIZE)/PAGE_SIZE * PAGE_SIZE))
                return NULL;
            
            manager.end += (size + ADD_SIZE)/PAGE_SIZE * PAGE_SIZE;
        }

        ((chunk)manager.start)->start = manager.start + BEF_DATA;
        ((chunk)manager.start)->prev = NULL;
        ((chunk)manager.start)->next = NULL;
        ((chunk)manager.start)->len = size;
        ((chunk)manager.start)->checksum = checksum((void*)manager.start);
        *(double*)(manager.start + CHUNK) = FENCE_VAL;
        *(double*)(manager.start + size + ADD_SIZE - CHUNK) = FENCE_VAL;
        manager.head = manager.start;

        return ((chunk)manager.start)->start;
            
    }
    else{

        chunk el = manager.head;

        if (manager.head != manager.start && size + ADD_SIZE <= manager.head - manager.start){

            ((chunk)manager.start)->start = manager.start + BEF_DATA;
            ((chunk)manager.start)->prev = NULL;
            ((chunk)manager.start)->next = el;
            ((chunk)manager.start)->len = size;
            ((chunk)manager.start)->checksum = checksum((void*)manager.start);
            el->prev = (chunk)manager.start;
            el = manager.start;
            *(double*)(manager.start + CHUNK) = FENCE_VAL;
            *(double*)(manager.start + size + ADD_SIZE - CHUNK) = FENCE_VAL;

            return ((chunk)manager.start)->start;
        }

        while(el != NULL){

            if (el->next == NULL && el + el->len + 2 * ADD_SIZE + size >= manager.end){

                if (-1 == custom_sbrk(((size + ADD_SIZE - manager.end + el->start + el->len + FENCE)/PAGE_SIZE + 1) * PAGE_SIZE))
                    return NULL;

                manager.end += ((size + ADD_SIZE - manager.end + el->start + el->len + FENCE)/PAGE_SIZE + 1) * PAGE_SIZE;
            }

            if (el->next == NULL || el + el->len + 2 * ADD_SIZE + size < el->next){

                if (el->next == NULL){

                    el->next = el + el->len + ADD_SIZE;
                    el->next->next = NULL;
                }
                else{

                    el->next->prev = (intptr_t)(el + el->len + ADD_SIZE);
                    ((chunk)(el + el->len + ADD_SIZE))->next = el->next;
                    el->next = el->next->prev;
                }

                el->next->prev = el;
                el->next->start = el->next + BEF_DATA;
                el->next->len = size;
                el->next->checksum = checksum((void*)el->next);
                *(double*)(el->next + CHUNK) = FENCE_VAL;
                *(double*)(el->next->start + el->next->len) = FENCE_VAL;

                return el->next->start;

            }

            el = el->next;
        }

    }

}

void* heap_calloc(size_t number, size_t size){

    return NULL;

}

void* heap_realloc(void* memblock, size_t count){

    return NULL;

}

void  heap_free(void* memblock){

    if (memblock != NULL){

        chunk el = manager.head;

        while(el != NULL){

            if (el == (chunk)(memblock - BEF_DATA)){

                if (el->next == NULL){

                    if (manager.end - (intptr_t)el >= PAGE_SIZE){

                        custom_sbrk(-((manager.end - (intptr_t)el)/PAGE_SIZE * PAGE_SIZE));
                        manager.end -= (manager.end - (intptr_t)el)/PAGE_SIZE * PAGE_SIZE;
                    }
                }
                else{



                }

                return;
            }

            el = el->next;
        }
    }
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
