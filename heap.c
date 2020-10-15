#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "heap.h"
#include "custom_unistd.h"
#define PAGE_SIZE 4096 // page length in bytes
#define FENCE 8 // fence length in bytes
#define CHUNK 32    //struct chunk size
#define CHUNK_CHECK 28 // length of memory to chunk truct sumcheck
#define ADD_SIZE (2 * FENCE + CHUNK)  // addictional allocation length
#define BEF_DATA (FENCE + CHUNK) // from pointer to first data index
#define FENCE_VAL 0xFFFFFFFFFFFFFFFF // value for fence validation
#define REMAINDER(ptr) ((intptr_t)(ptr) & (intptr_t)(PAGE_SIZE - 1)) // calculate remainder of page allignment

heap manager = {.start = 0, .end = 0, .head = NULL};

int heap_setup(void){

    void * a = custom_sbrk(PAGE_SIZE);

    if ((intptr_t)a == -1)
        return -1;

    manager.start = (intptr_t)a;
    manager.end = (intptr_t)a + PAGE_SIZE;
    manager.head = NULL;

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

void* heap_malloc(size_t size){

    if (manager.start == manager.end || size == 0 || heap_validate())
        return NULL;

    if (manager.head == NULL){

        if ((intptr_t)(manager.start + size + ADD_SIZE) > (intptr_t)manager.end){
            
            if (-1 == (intptr_t)custom_sbrk((size + ADD_SIZE)/PAGE_SIZE * PAGE_SIZE))
                return NULL;
            
            manager.end += (size + ADD_SIZE)/PAGE_SIZE * PAGE_SIZE;
        }

        manager.head = (chunk)manager.start;
        manager.head->start = manager.start + BEF_DATA;
        manager.head->prev = NULL;
        manager.head->next = NULL;
        manager.head->len = size;
        manager.head->checksum = checksum((void*)manager.start);
        *((double*)(manager.start + CHUNK)) = FENCE_VAL;
        *((double*)(manager.start + size + ADD_SIZE - CHUNK)) = FENCE_VAL;

        return (void*)manager.head->start;
            
    }
    else{

        chunk el = manager.head;

        if ((intptr_t)manager.head != manager.start && (intptr_t)(manager.start + size + ADD_SIZE) <= (intptr_t)manager.head){

            el->prev = (chunk)manager.start;
            el->prev->start = manager.start + BEF_DATA;
            el->prev->prev = NULL;
            el->prev->next = el;
            el->prev->len = size;
            el->prev->checksum = checksum((void*)manager.start);
            el = (chunk)manager.start;
            *((double*)(manager.start + CHUNK)) = FENCE_VAL;
            *((double*)(manager.start + size + ADD_SIZE - CHUNK)) = FENCE_VAL;

            return (void*)el->prev->start;
        }

        while(el != NULL){

            if (el->next == NULL && (intptr_t)(el + el->len + 2 * ADD_SIZE + size) > manager.end){

                if (-1 == (intptr_t)custom_sbrk(((size + ADD_SIZE - manager.end + el->start + el->len + FENCE)/PAGE_SIZE + 1) * PAGE_SIZE))
                    return NULL;

                manager.end += ((size + ADD_SIZE - manager.end + el->start + el->len + FENCE - 1)/PAGE_SIZE + 1) * PAGE_SIZE;
            }

            if (el->next == NULL || (intptr_t)(el + el->len + 2 * ADD_SIZE + size) <= (intptr_t)(el->next)){

                if (el->next == NULL){

                    el->next = (chunk)(el + el->len + ADD_SIZE);
                    el->next->next = NULL;
                }
                else{

                    el->next->prev = (chunk)(el + el->len + ADD_SIZE);
                    el->next->prev->next = el->next;
                    el->next = el->next->prev;
                }

                el->next->prev = el;
                el->next->start = (intptr_t)(el->next + BEF_DATA);
                el->next->len = size;
                el->next->checksum = checksum((void*)el->next);
                *((double*)(el->next + CHUNK)) = FENCE_VAL;
                *((double*)(el->next->start + el->next->len)) = FENCE_VAL;

                return (void*)el->next->start;

            }

            el = el->next;
        }
    }

    return NULL;
}

void* heap_calloc(size_t number, size_t size){

    void * pointer = heap_malloc(number*size);
    if (pointer == NULL)
        return NULL;
    return memset(pointer, 0, number*size);
}

void* heap_realloc(void* memblock, size_t size){

    if ((memblock == NULL && size == 0) || manager.head == NULL && memblock != NULL || (memblock != NULL && pointer_valid != get_pointer_type(memblock)) || heap_validate())
        return NULL;

    if (memblock != NULL && size == 0){

        heap_free(memblock);
        return NULL;
    }
    
    if (memblock == NULL && size != 0){

        return heap_malloc(size);
    }

    if (((chunk)((intptr_t)memblock - BEF_DATA))->len >= size){

        ((chunk)((intptr_t)memblock - BEF_DATA))->len = size;
        ((chunk)((intptr_t)memblock - BEF_DATA))->checksum = checksum((void*)((intptr_t)memblock - BEF_DATA));

        return memblock;
    }

    chunk el = manager.head;

    while(el != NULL){

        if (el->start == (intptr_t)memblock){

            if (el->next == NULL){

                if ((intptr_t)(el->start + size + FENCE) > manager.end){

                    if (el->prev)
                        el->prev->next = NULL;
                    else
                        manager.head = NULL;
                    
                    void * alloc = heap_malloc(size);

                    if (alloc == NULL){

                        if (el->prev)
                            el->prev->next = el;
                        else
                            manager.head = el;

                        return NULL;
                    }

                    return memcpy(alloc, (void*)el->start, el->len);
                }
                else{

                    el->len = size;
                    el->checksum = checksum((void*)el);
                    *((double*)(el->start + size)) = FENCE_VAL;

                    return (void*)el->start;
                }
            }
            else{

                if ((intptr_t)(el->start + size + FENCE) > (intptr_t)el->next){

                    if (el->prev){
                        el->prev->next = el->next;
                        el->next->prev = el->prev;
                    }
                    else{
                        manager.head = el->next;
                        el->next->prev = NULL;
                    }
                    
                    void * alloc = heap_malloc(size);

                    if (alloc == NULL){

                        if (el->prev){
                            el->prev->next = el;
                            el->next->prev = el;
                        }
                        else{
                            manager.head = el;
                            el->next->prev = el;
                        }

                        return NULL;
                    }

                    return memcpy(alloc, (void*)el->start, el->len);
                }
                else{

                    el->len = size;
                    el->checksum = checksum((void*)el);
                    *((double*)(el->start + size)) = FENCE_VAL;

                    return (void*)el->start;
                }
            }
        }

        el = el->next;
    }

    return NULL;
}

void  heap_free(void* memblock){

    if (memblock != NULL && pointer_valid == get_pointer_type(memblock)){   //get pointer takes const void. Err may occur

        chunk el = manager.head;

        while(el != NULL){

            if ((intptr_t)el == (intptr_t)memblock - BEF_DATA){

                if (el->next == NULL){

                    if (el->prev){
                        el->prev->next = NULL;
                    }
                    else{
                        manager.head = NULL;
                    }

                    if (manager.end - (intptr_t)el >= PAGE_SIZE){

                        custom_sbrk(-((manager.end - (intptr_t)el)/PAGE_SIZE * PAGE_SIZE));
                        manager.end -= (manager.end - (intptr_t)el)/PAGE_SIZE * PAGE_SIZE;
                    }

                }
                else{

                    if (el->prev){
                        el->prev->next = el->next;
                        el->next->prev = el->prev;
                    }
                    else{
                        el->next->prev = NULL;
                        manager.head = el->next;
                    }
                }

                return;
            }

            el = el->next;
        }
    }
}

size_t heap_get_largest_used_block_size(void){

    size_t a = 0;
    
    if (manager.start != manager.end && manager.head != NULL && !heap_validate()){

        chunk el = manager.head;
        
        while(el != NULL){

            if (el->len > a){
                a = el->len;
            }

            el = el->next;
        }
    }
    
    return a;
}

enum pointer_type_t get_pointer_type(const void* const pointer){

    if (pointer == NULL)
        return pointer_null;
    
    if (heap_validate()){
        return pointer_heap_corrupted;
    }

    if (manager.head != NULL){

        chunk el = manager.head;

        while(el != NULL){

            if ((intptr_t)pointer >= (intptr_t)el && (intptr_t)pointer < (intptr_t)el + CHUNK)
                return pointer_control_block;

            if ((intptr_t)pointer == (intptr_t)el + BEF_DATA)
                return pointer_valid;

            if ((intptr_t)pointer > (intptr_t)el + BEF_DATA && (intptr_t)pointer < (intptr_t)el + BEF_DATA + el->len)
                return pointer_inside_data_block;

            if (((intptr_t)pointer >= (intptr_t)el + CHUNK && (intptr_t)pointer < (intptr_t)el + BEF_DATA) || ((intptr_t)pointer >= (intptr_t)el->start + el->len && (intptr_t)pointer < (intptr_t)el + el->len + ADD_SIZE))
                return pointer_inside_fences;

            if (el->next != NULL){

                if ((intptr_t)pointer >= (intptr_t)el + el->len + ADD_SIZE && (intptr_t)pointer < (intptr_t)el->next)
                    return pointer_unallocated;
            }
            else{

                if ((intptr_t)pointer >= (intptr_t)el + el->len + ADD_SIZE && (intptr_t)pointer < manager.end)
                    return pointer_unallocated;
            }
        }
    }
    else{

        if (manager.start != manager.end)
            return pointer_unallocated;     //I am not sure
    }

    return pointer_unallocated;
}

int heap_validate(void){

    if (manager.end <= manager.start || REMAINDER(manager.end - manager.start) != 0)
        return 2;

    if (manager.head != NULL){

        chunk el = manager.head;

        while (el != NULL){

            if (checksum((void*)el) != el->checksum)
                return 3;

            if (*((double*)(el + CHUNK)) != FENCE_VAL || *((double*)(el + el->len + BEF_DATA)) != FENCE_VAL)
                return 1;

            el = el->next;
        }
    }

    return 0;
}

void* heap_malloc_aligned(size_t size){

    if (manager.start == manager.end || size == 0 || heap_validate())
        return NULL;

    if (manager.head == NULL){

        if ((intptr_t)(manager.start + PAGE_SIZE + size + FENCE) > manager.end){
            
            if ((intptr_t)-1 == (intptr_t)custom_sbrk((PAGE_SIZE + size + FENCE - 1)/PAGE_SIZE * PAGE_SIZE))
                return NULL;
            
            manager.end += (PAGE_SIZE + size + FENCE - 1)/PAGE_SIZE * PAGE_SIZE;
        }

        manager.head = (chunk)(manager.start + PAGE_SIZE - BEF_DATA);
        manager.head->start = manager.start + PAGE_SIZE;
        manager.head->prev = NULL;
        manager.head->next = NULL;
        manager.head->len = size;
        manager.head->checksum = checksum((void*)manager.head);
        *((double*)(manager.head + CHUNK)) = FENCE_VAL;
        *((double*)(manager.head + size + ADD_SIZE - CHUNK)) = FENCE_VAL;

        return (void*)manager.head->start;
            
    }
    else{

        chunk el = manager.head;

        if ((intptr_t)manager.head != (intptr_t)manager.start && (intptr_t)(manager.start + PAGE_SIZE + size + FENCE) <= (intptr_t)manager.head){

            manager.head->prev = (chunk)(manager.start + PAGE_SIZE - BEF_DATA);
            manager.head->prev->next = manager.head;
            manager.head = manager.head->prev;
            manager.head->start = manager.start + PAGE_SIZE;
            manager.head->prev = NULL;
            manager.head->len = size;
            manager.head->checksum = checksum((void*)manager.head);
            *((double*)(manager.head + CHUNK)) = FENCE_VAL;
            *((double*)(manager.head + size + ADD_SIZE - CHUNK)) = FENCE_VAL;

            return (void*)manager.head->start;
        }

        while(el != NULL){

            if (el->next == NULL){

                if ((intptr_t)(PAGE_SIZE - REMAINDER(el + ADD_SIZE + el->len)) >= BEF_DATA){

                    if ((intptr_t)-1 == (intptr_t)custom_sbrk(((size + FENCE - 1)/PAGE_SIZE + 1) * PAGE_SIZE))
                        return NULL;

                    manager.end += ((size + FENCE - 1)/PAGE_SIZE + 1) * PAGE_SIZE;
                    el->next = el + el->len + ADD_SIZE - REMAINDER(el + el->len + ADD_SIZE) + PAGE_SIZE - BEF_DATA;
                }
                else{

                    if ((intptr_t)-1 == (intptr_t)custom_sbrk(((size + FENCE - 1)/PAGE_SIZE + 2) * PAGE_SIZE))
                        return NULL;

                    manager.end += ((size + FENCE - 1)/PAGE_SIZE + 2) * PAGE_SIZE;
                    el->next = el + el->len + ADD_SIZE - REMAINDER(el + el->len + ADD_SIZE) + 2 * PAGE_SIZE - BEF_DATA;
                }
                
                el->next->prev = el;
                el->next->next = NULL;
                el->next->len = size;
                el->next->start = (intptr_t)(el->next + BEF_DATA);
                el->next->checksum = checksum((void*)el->next);
                *((double*)(el->next + CHUNK)) = FENCE_VAL;
                *((double*)(el->next + size + ADD_SIZE - CHUNK)) = FENCE_VAL;

                return (void*)el->next->start;
            }
            else{

                int x = 2;

                if (PAGE_SIZE - REMAINDER(el + ADD_SIZE + el->len) >= BEF_DATA)
                    x = 1;

                if ((intptr_t)(el + el->len + ADD_SIZE - REMAINDER(el + el->len + ADD_SIZE) + 2 * PAGE_SIZE + size + FENCE) <= (intptr_t)el->next){

                    el->next->prev = el + el->len + ADD_SIZE - REMAINDER(el + el->len + ADD_SIZE) + x * PAGE_SIZE - BEF_DATA;
                    el->next->prev->next = el->next;
                    el->next = el->next->prev;
                    el->next->prev = el;
                    el->next->len = size;
                    el->next->start = (intptr_t)(el->next + BEF_DATA);
                    el->next->checksum = checksum((void*)el->next);
                    *((double*)(el->next + CHUNK)) = FENCE_VAL;
                    *((double*)(el->next + size + ADD_SIZE - CHUNK)) = FENCE_VAL;

                    return (void*)el->next->start;
                }
            }

            el = el->next;
        }
    }

    return NULL;
}

void* heap_calloc_aligned(size_t number, size_t size){

    void * pointer = heap_malloc_aligned(number*size);
    if (pointer == NULL)
        return NULL;
    return memset(pointer, 0, number*size);
}

void* heap_realloc_aligned(void* memblock, size_t size){

    if ((memblock == NULL && size == 0) || manager.head == NULL && memblock != NULL || (memblock != NULL && pointer_valid != get_pointer_type(memblock)) || heap_validate())
        return NULL;

    if (memblock != NULL && size == 0){

        heap_free(memblock);
        return NULL;
    }
    
    if (memblock == NULL && size != 0){

        return heap_malloc_aligned(size);
    }

    if (((chunk)((intptr_t)memblock - BEF_DATA))->len >= size){

        ((chunk)((intptr_t)memblock - BEF_DATA))->len = size;
        ((chunk)((intptr_t)memblock - BEF_DATA))->checksum = checksum((void*)((intptr_t)memblock - BEF_DATA));
        return memblock;
    }

    chunk el = manager.head;

    while(el != NULL){

        if (el->start == (intptr_t)memblock){

            if (el->next == NULL){

                if ((intptr_t)(el->start + size + FENCE) > manager.end){

                    if (el->prev)
                        el->prev->next = NULL;
                    else
                        manager.head = NULL;
                    
                    void * alloc = heap_malloc_aligned(size);

                    if (alloc == NULL){

                        if (el->prev)
                            el->prev->next = el;
                        else
                            manager.head = el;

                        return NULL;
                    }

                    return memcpy(alloc, (void*)el->start, el->len);
                }
                else{

                    el->len = size;
                    el->checksum = checksum((void*)el);
                    *((double*)(el->start + size)) = FENCE_VAL;

                    return (void*)el->start;
                }
            }
            else{

                if ((intptr_t)(el->start + size + FENCE) > (intptr_t)el->next){

                    if (el->prev){
                        el->prev->next = el->next;
                        el->next->prev = el->prev;
                    }
                    else{
                        manager.head = el->next;
                        el->next->prev = NULL;
                    }
                    
                    void * alloc = heap_malloc_aligned(size);

                    if (alloc == NULL){

                        if (el->prev){
                            el->prev->next = el;
                            el->next->prev = el;
                        }
                        else{
                            manager.head = el;
                            el->next->prev = el;
                        }

                        return NULL;
                    }

                    return memcpy(alloc, (void*)el->start, el->len);
                }
                else{

                    el->len = size;
                    el->checksum = checksum((void*)el);
                    *((double*)(el->start + size)) = FENCE_VAL;

                    return (void*)el->start;
                }
            }
        }

        el = el->next;
    }

    return NULL;
}

