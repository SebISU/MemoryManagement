#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include "heap.h"
#include "custom_unistd.h"
#define PAGE_SIZE 4096 // page length in bytes
#define FENCE 8 // fence length in bytes
#define CHUNK 48    //struct chunk size
#define CHUNK_CHECK (CHUNK - 8) // length of memory to chunk truct sumcheck
#define ADD_SIZE (2 * FENCE + CHUNK)  // addictional allocation length
#define BEF_DATA (FENCE + CHUNK) // from pointer to first data index
#define REMAINDER(ptr) ((intptr_t)(ptr) & (intptr_t)(PAGE_SIZE - 1)) // calculate remainder of page allignment
#define WRD_LEN ((int)sizeof(void*)) // machine word length
#define WRD_ALL(ptr) ((intptr_t)(ptr) & (intptr_t)(WRD_LEN - 1))    // calculate remainder of machine word allignment


heap manager = {.start = 0, .end = 0, .head = NULL};

int heap_setup(void){

    void * a = custom_sbrk(PAGE_SIZE);

    if ((intptr_t)a == -1)
        return -1;

    manager.start = (intptr_t)a;
    manager.end = (intptr_t)a + PAGE_SIZE;
    manager.head = NULL;

    int status = pthread_mutexattr_init(&manager.attrs);

    if (status)
        return -1;

    status = pthread_mutexattr_settype(&manager.attrs, PTHREAD_MUTEX_RECURSIVE);

    if (status)
        return -1;

    status = pthread_mutex_init(&manager.mutex, &manager.attrs);

    if (status)
        return -1;

    return 0;
}

void heap_clean(void){

    custom_sbrk(-(manager.end - manager.start));
    manager.start = 0;
    manager.end = 0;

    manager.head = NULL;

    pthread_mutex_destroy(&manager.mutex);

    pthread_mutexattr_destroy(&manager.attrs);

}

unsigned short checksum(void * str){

    pthread_mutex_lock(&manager.mutex);

    if (str == NULL){

        pthread_mutex_unlock(&manager.mutex);
        return -1;
    }

    unsigned short a = 0;
    int x;

    for(x = 0; x < CHUNK_CHECK; x++){

        a += ((uint8_t*)str)[x];
    }

    for (x += 2; x < CHUNK; x++){

        a += ((uint8_t*)str)[x];
    }

    pthread_mutex_unlock(&manager.mutex);
    return a;
}

void* heap_malloc(size_t size){

    pthread_mutex_lock(&manager.mutex);

    if (manager.start == manager.end || size == 0 || heap_validate()){
    
        pthread_mutex_unlock(&manager.mutex);
        return NULL;
    }

    if (manager.head == NULL){

        if ((intptr_t)(manager.start + size + ADD_SIZE) > manager.end){
            
            if (-1 == (intptr_t)custom_sbrk((size + ADD_SIZE)/PAGE_SIZE * PAGE_SIZE)){
            
                pthread_mutex_unlock(&manager.mutex);
                return NULL;
            }
            
            manager.end += (size + ADD_SIZE)/PAGE_SIZE * PAGE_SIZE;
        }

        manager.head = (chunk)manager.start;
        manager.head->start = manager.start + BEF_DATA;
        manager.head->prev = NULL;
        manager.head->next = NULL;
        manager.head->len = size;
        manager.head->checksum = checksum((void*)manager.start);
        *((uint64_t*)((uint8_t*)manager.head + CHUNK)) = UINT64_MAX;
        *((uint64_t*)((uint8_t*)manager.head + size + ADD_SIZE - FENCE)) = UINT64_MAX;

        pthread_mutex_unlock(&manager.mutex);
        return (void*)manager.head->start;
            
    }
    else{

        chunk el = manager.head;

        if ((intptr_t)el != manager.start && (intptr_t)(manager.start + size + ADD_SIZE) <= (intptr_t)el){

            el->prev = (chunk)manager.start;
            el->prev->start = manager.start + BEF_DATA;
            el->prev->prev = NULL;
            el->prev->next = el;
            el->prev->len = size;
            el->prev->checksum = checksum((void*)el->prev);
            el->checksum = checksum((void*)el);
            manager.head = el->prev;
            *((uint64_t*)((uint8_t*)el->prev + CHUNK)) = UINT64_MAX;
            *((uint64_t*)((uint8_t*)el->prev + size + ADD_SIZE - FENCE)) = UINT64_MAX;

            pthread_mutex_unlock(&manager.mutex);
            return (void*)el->prev->start;
        }

        while(el != NULL){

            int x = WRD_ALL((char*)el + el->len + ADD_SIZE);

            if (x){
                x = WRD_LEN - x;
            }

            if (el->next == NULL && (intptr_t)((uint8_t*)el + el->len + 2 * ADD_SIZE + size + x) > manager.end){

                if (-1 == (intptr_t)custom_sbrk(((size + ADD_SIZE - manager.end + el->start + el->len + FENCE + x)/PAGE_SIZE + 1) * PAGE_SIZE)){
                
                    pthread_mutex_unlock(&manager.mutex);
                    return NULL;
                }

                manager.end += ((size + ADD_SIZE - manager.end + el->start + el->len + FENCE + x)/PAGE_SIZE + 1) * PAGE_SIZE;
            }

            if (el->next == NULL || (intptr_t)((uint8_t*)el + el->len + 2 * ADD_SIZE + size + x) <= (intptr_t)(el->next)){

                if (el->next == NULL){

                    el->next = (chunk)((uint8_t*)el + el->len + ADD_SIZE + x);
                    el->next->next = NULL;
                }
                else{

                    el->next->prev = (chunk)((uint8_t*)el + el->len + ADD_SIZE + x);
                    el->next->checksum = checksum((void*)el->next);
                    el->next->prev->next = el->next;
                    el->next = el->next->prev;
                }

                el->next->prev = el;
                el->next->start = (intptr_t)el->next + BEF_DATA;
                el->next->len = size;
                el->checksum = checksum((void*)el);
                el->next->checksum = checksum((void*)el->next);
                *((uint64_t*)((uint8_t*)el->next + CHUNK)) = UINT64_MAX;
                *((uint64_t*)(el->next->start + el->next->len)) = UINT64_MAX;

                pthread_mutex_unlock(&manager.mutex);
                return (void*)el->next->start;

            }

            el = el->next;
        }
    }

    pthread_mutex_unlock(&manager.mutex);
    return NULL;
}

void* heap_calloc(size_t number, size_t size){

    pthread_mutex_lock(&manager.mutex);

    void * ptr = heap_malloc(number*size);
    
    if (ptr == NULL){
    
        pthread_mutex_unlock(&manager.mutex);
        return NULL;
    }

    ptr = memset(ptr, 0, number*size);

    pthread_mutex_unlock(&manager.mutex);
    return ptr;
}

void* heap_realloc(void* memblock, size_t size){

    pthread_mutex_lock(&manager.mutex);

    if ((memblock == NULL && size == 0) || manager.head == NULL && memblock != NULL || (memblock != NULL && pointer_valid != get_pointer_type(memblock)) || heap_validate()){
    
        pthread_mutex_unlock(&manager.mutex);
        return NULL;
    }

    if (memblock != NULL && size == 0){

        heap_free(memblock);

        pthread_mutex_unlock(&manager.mutex);
        return NULL;
    }
    
    if (memblock == NULL && size != 0){

        void * ptr = heap_malloc(size);

        pthread_mutex_unlock(&manager.mutex);
        return ptr;
    }

    if (((chunk)((intptr_t)memblock - BEF_DATA))->len >= size){

        ((chunk)((intptr_t)memblock - BEF_DATA))->len = size;
        ((chunk)((intptr_t)memblock - BEF_DATA))->checksum = checksum((void*)((intptr_t)memblock - BEF_DATA));
        *((uint64_t*)((uint8_t*)memblock + size)) = UINT64_MAX;

        if (((chunk)((intptr_t)memblock - BEF_DATA))->next == NULL && manager.end - (intptr_t)memblock - ((chunk)((uint8_t*)memblock - BEF_DATA))->len - FENCE > PAGE_SIZE){

            custom_sbrk(-((manager.end - (intptr_t)memblock - ((chunk)((uint8_t*)memblock - BEF_DATA))->len - FENCE)/PAGE_SIZE * PAGE_SIZE));
            manager.end -= (manager.end - (intptr_t)memblock - ((chunk)((uint8_t*)memblock - BEF_DATA))->len - FENCE)/PAGE_SIZE * PAGE_SIZE;
        }

        pthread_mutex_unlock(&manager.mutex);
        return memblock;
    }

    chunk el = manager.head;

    while(el != NULL){

        if (el->start == (intptr_t)memblock){

            if (el->next == NULL){

                if ((intptr_t)(el->start + size + FENCE) > manager.end){

                    if (-1 == (intptr_t)custom_sbrk(((size - el->len - PAGE_SIZE + REMAINDER((uint8_t*)el->start + el->len))/PAGE_SIZE + 1) * PAGE_SIZE)){

                        void * alloc = heap_malloc(size);

                        if (alloc != NULL){

                            alloc = memmove(alloc, (void*)el->start, el->len);

                            heap_free((void*)el->start);

                            pthread_mutex_unlock(&manager.mutex);
                            return alloc;
                        }

                        pthread_mutex_unlock(&manager.mutex);
                        return NULL;
                    }

                    manager.end += ((size - el->len - PAGE_SIZE + REMAINDER((uint8_t*)el->start + el->len))/PAGE_SIZE + 1) * PAGE_SIZE;
                    el->len = size;
                    el->checksum = checksum(el);
                    *((uint64_t*)(el->start + size)) = UINT64_MAX;

                    pthread_mutex_unlock(&manager.mutex);
                    return (void*)el->start;

                }
                else{

                    el->len = size;
                    el->checksum = checksum((void*)el);
                    *((uint64_t*)(el->start + size)) = UINT64_MAX;

                    pthread_mutex_unlock(&manager.mutex);
                    return (void*)el->start;
                }
            }
            else{

                if ((intptr_t)(el->start + size + FENCE) > (intptr_t)el->next){

                    void * alloc = heap_malloc(size);

                    if (alloc != NULL){

                        alloc = memmove(alloc, (void*)el->start, el->len);

                        heap_free((void*)el->start);

                        pthread_mutex_unlock(&manager.mutex);
                        return alloc;
                    }

                    pthread_mutex_unlock(&manager.mutex);
                    return NULL;

                }
                else{

                    el->len = size;
                    el->checksum = checksum((void*)el);
                    *((uint64_t*)(el->start + size)) = UINT64_MAX;

                    pthread_mutex_unlock(&manager.mutex);
                    return (void*)el->start;
                }
            }
        }

        el = el->next;
    }

    pthread_mutex_unlock(&manager.mutex);
    return NULL;
}

void  heap_free(void* memblock){

    pthread_mutex_lock(&manager.mutex);

    if (memblock != NULL && pointer_valid == get_pointer_type(memblock)){

        chunk el = manager.head;

        while(el != NULL){

            if ((intptr_t)el->start == (intptr_t)memblock){

                if (el->next == NULL){

                    if (el->prev){
                        el->prev->next = NULL;
                        el->prev->checksum = checksum(el->prev);
                    }
                    else{
                        manager.head = NULL;
                    }

                    if (manager.end - (intptr_t)el >= PAGE_SIZE && manager.end - manager.start > PAGE_SIZE){

                        if ((intptr_t)el == manager.start){

                            custom_sbrk(-((manager.end - (intptr_t)el - 1)/PAGE_SIZE * PAGE_SIZE));
                            manager.end -= (manager.end - (intptr_t)el - 1)/PAGE_SIZE * PAGE_SIZE;
                        }
                        else{

                            custom_sbrk(-((manager.end - (intptr_t)el)/PAGE_SIZE * PAGE_SIZE));
                            manager.end -= (manager.end - (intptr_t)el)/PAGE_SIZE * PAGE_SIZE;
                        }
                    }

                }
                else{

                    if (el->prev){
                        el->prev->next = el->next;
                        el->next->prev = el->prev;
                        el->prev->checksum = checksum(el->prev);
                        el->next->checksum = checksum(el->next);
                    }
                    else{
                        el->next->prev = NULL;
                        el->next->checksum = checksum(el->next);
                        manager.head = el->next;
                    }
                }

                pthread_mutex_unlock(&manager.mutex);
                return;
            }

            el = el->next;
        }
    }

    pthread_mutex_unlock(&manager.mutex);
}

size_t heap_get_largest_used_block_size(void){

    size_t a = 0;
    
    pthread_mutex_lock(&manager.mutex);

    if (manager.start != manager.end && manager.head != NULL && !heap_validate()){

        chunk el = manager.head;
        
        while(el != NULL){

            if (el->len > a){
                a = el->len;
            }

            el = el->next;
        }
    }
    
    pthread_mutex_unlock(&manager.mutex);
    return a;
}

enum pointer_type_t get_pointer_type(const void* const pointer){

    pthread_mutex_lock(&manager.mutex);

    if (pointer == NULL){

        pthread_mutex_unlock(&manager.mutex);
        return pointer_null;
    }

    if ((intptr_t)pointer < manager.start || (intptr_t)pointer >= manager.end){

        pthread_mutex_unlock(&manager.mutex);
        return pointer_unallocated;
    }
    
    if (heap_validate()){

        pthread_mutex_unlock(&manager.mutex);
        return pointer_heap_corrupted;
    }

    if (manager.head != NULL){

        if ((intptr_t)manager.head != manager.start && (intptr_t)pointer >= manager.start && (intptr_t)pointer < (intptr_t)manager.head){

            pthread_mutex_unlock(&manager.mutex);
            return pointer_unallocated;
        }

        chunk el = manager.head;

        while(el != NULL){

            if ((intptr_t)pointer >= (intptr_t)el && (intptr_t)pointer < (intptr_t)((uint8_t*)el + CHUNK)){
            
                pthread_mutex_unlock(&manager.mutex);
                return pointer_control_block;
            }

            if ((intptr_t)pointer == (intptr_t)((uint8_t*)el + BEF_DATA)){
            
                pthread_mutex_unlock(&manager.mutex);
                return pointer_valid;
            }

            if ((intptr_t)pointer > (intptr_t)((uint8_t*)el + BEF_DATA) && (intptr_t)pointer < (intptr_t)((uint8_t*)el + BEF_DATA + el->len)){
            
                pthread_mutex_unlock(&manager.mutex);
                return pointer_inside_data_block;
            }

            if (((intptr_t)pointer >= (intptr_t)((uint8_t*)el + CHUNK) && (intptr_t)pointer < (intptr_t)((uint8_t*)el + BEF_DATA)) || ((intptr_t)pointer >= (intptr_t)(el->start + el->len) && (intptr_t)pointer < (intptr_t)((uint8_t*)el + el->len + ADD_SIZE))){
            
                pthread_mutex_unlock(&manager.mutex);
                return pointer_inside_fences;
            }

            if (el->next != NULL){

                if ((intptr_t)pointer >= (intptr_t)((uint8_t*)el + el->len + ADD_SIZE) && (intptr_t)pointer < (intptr_t)el->next){
                
                    pthread_mutex_unlock(&manager.mutex);
                    return pointer_unallocated;
                }
            }
            else{

                if ((intptr_t)pointer >= (intptr_t)((uint8_t*)el + el->len + ADD_SIZE) && (intptr_t)pointer < manager.end){
                
                    pthread_mutex_unlock(&manager.mutex);
                    return pointer_unallocated;
                }
            }
            
            el = el->next;
        }
    }

    pthread_mutex_unlock(&manager.mutex);
    return pointer_unallocated;
}

int heap_validate(void){

    pthread_mutex_lock(&manager.mutex);

    if (manager.end <= manager.start || REMAINDER(manager.end - manager.start) != 0){
        
        pthread_mutex_unlock(&manager.mutex);
        return 2;
    }

    if (manager.head != NULL){

        chunk el = manager.head;

        while (el != NULL){

            if (checksum((void*)el) != el->checksum){

                pthread_mutex_unlock(&manager.mutex);
                return 3;
            }
            else if (*((uint64_t*)((uint8_t*)el + CHUNK)) ^ UINT64_MAX || *((uint64_t*)((uint8_t*)el + el->len + BEF_DATA)) ^ UINT64_MAX){

                pthread_mutex_unlock(&manager.mutex);
                return 1;
            }

            el = el->next;
        }
    }

    pthread_mutex_unlock(&manager.mutex);
    return 0;
}

void* heap_malloc_aligned(size_t size){

    pthread_mutex_lock(&manager.mutex);

    if (manager.start == manager.end || size == 0 || heap_validate()){

        pthread_mutex_unlock(&manager.mutex);
        return NULL;
    }

    if (manager.head == NULL){

        if ((intptr_t)(manager.start + PAGE_SIZE + size + FENCE) > manager.end){
            
            if (-1 == (intptr_t)custom_sbrk((PAGE_SIZE + size + FENCE - 1)/PAGE_SIZE * PAGE_SIZE)){
            
                pthread_mutex_unlock(&manager.mutex);
                return NULL;
            }
            
            manager.end += (PAGE_SIZE + size + FENCE - 1)/PAGE_SIZE * PAGE_SIZE;
        }

        manager.head = (chunk)(manager.start + PAGE_SIZE - BEF_DATA);
        manager.head->start = manager.start + PAGE_SIZE;
        manager.head->prev = NULL;
        manager.head->next = NULL;
        manager.head->len = size;
        manager.head->checksum = checksum((void*)manager.head);
        *((uint64_t*)((uint8_t*)manager.head + CHUNK)) = UINT64_MAX;
        *((uint64_t*)((uint8_t*)manager.head + size + ADD_SIZE - FENCE)) = UINT64_MAX;

        pthread_mutex_unlock(&manager.mutex);
        return (void*)manager.head->start;
            
    }
    else{

        chunk el = manager.head;

        if ((intptr_t)el != (intptr_t)manager.start && (intptr_t)(manager.start + PAGE_SIZE + size + FENCE) <= (intptr_t)el){

            el->prev = (chunk)(manager.start + PAGE_SIZE - BEF_DATA);
            el->prev->next = el;
            el->prev->start = manager.start + PAGE_SIZE;
            el->prev->prev = NULL;
            el->prev->len = size;
            el->prev->checksum = checksum((void*)el->prev);
            el->checksum = checksum((void*)el);
            manager.head = el->prev;
            *((uint64_t*)((uint8_t*)el->prev + CHUNK)) = UINT64_MAX;
            *((uint64_t*)((uint8_t*)el->prev + size + ADD_SIZE - FENCE)) = UINT64_MAX;

            pthread_mutex_unlock(&manager.mutex);
            return (void*)el->prev->start;
        }

        while(el != NULL){

            if (el->next == NULL){

                if ((intptr_t)(PAGE_SIZE - REMAINDER((uint8_t*)el + ADD_SIZE + el->len)) >= BEF_DATA){

                    if (-1 == (intptr_t)custom_sbrk(((size + FENCE - 1)/PAGE_SIZE + 1) * PAGE_SIZE)){
                    
                        pthread_mutex_unlock(&manager.mutex);
                        return NULL;
                    }

                    manager.end += ((size + FENCE - 1)/PAGE_SIZE + 1) * PAGE_SIZE;
                    el->next = (chunk)((uint8_t*)el + el->len + ADD_SIZE - REMAINDER((uint8_t*)el + el->len + ADD_SIZE) + PAGE_SIZE - BEF_DATA);
                }
                else{

                    if ((intptr_t)-1 == (intptr_t)custom_sbrk(((size + FENCE - 1)/PAGE_SIZE + 2) * PAGE_SIZE)){
                    
                        pthread_mutex_unlock(&manager.mutex);
                        return NULL;
                    }

                    manager.end += ((size + FENCE - 1)/PAGE_SIZE + 2) * PAGE_SIZE;
                    el->next = (chunk)((uint8_t*)el + el->len + ADD_SIZE - REMAINDER((uint8_t*)el + el->len + ADD_SIZE) + 2 * PAGE_SIZE - BEF_DATA);
                }
                
                el->next->prev = el;
                el->next->next = NULL;
                el->next->len = size;
                el->next->start = (intptr_t)el->next + BEF_DATA;
                el->checksum = checksum((void*)el);
                el->next->checksum = checksum((void*)el->next);
                *((uint64_t*)((uint8_t*)el->next + CHUNK)) = UINT64_MAX;
                *((uint64_t*)((uint8_t*)el->next + size + ADD_SIZE - FENCE)) = UINT64_MAX;

                pthread_mutex_unlock(&manager.mutex);
                return (void*)el->next->start;
            }
            else{

                int x = 2;

                if (PAGE_SIZE - REMAINDER((uint8_t*)el + ADD_SIZE + el->len) >= BEF_DATA)
                    x = 1;

                if ((intptr_t)((uint8_t*)el + el->len + ADD_SIZE - REMAINDER((uint8_t*)el + el->len + ADD_SIZE) + x * PAGE_SIZE + size + FENCE) <= (intptr_t)el->next){

                    el->next->prev = (chunk)((uint8_t*)el + el->len + ADD_SIZE - REMAINDER((uint8_t*)el + el->len + ADD_SIZE) + x * PAGE_SIZE - BEF_DATA);
                    el->next->checksum = checksum((void*)el->next);
                    el->next->prev->next = el->next;
                    el->next = el->next->prev;
                    el->next->prev = el;
                    el->next->len = size;
                    el->next->start = (intptr_t)el->next + BEF_DATA;
                    el->next->checksum = checksum((void*)el->next);
                    el->checksum = checksum((void*)el);
                    *((uint64_t*)((uint8_t*)el->next + CHUNK)) = UINT64_MAX;
                    *((uint64_t*)((uint8_t*)el->next + size + ADD_SIZE - FENCE)) = UINT64_MAX;

                    pthread_mutex_unlock(&manager.mutex);
                    return (void*)el->next->start;
                }
            }

            el = el->next;
        }
    }

    pthread_mutex_unlock(&manager.mutex);
    return NULL;
}

void* heap_calloc_aligned(size_t number, size_t size){

    pthread_mutex_lock(&manager.mutex);

    void * ptr = heap_malloc_aligned(number*size);

    if (ptr == NULL){
    
        pthread_mutex_unlock(&manager.mutex);
        return NULL;
    }

    ptr = memset(ptr, 0, number*size);

    pthread_mutex_unlock(&manager.mutex);
    return ptr;
}

void* heap_realloc_aligned(void* memblock, size_t size){

    pthread_mutex_lock(&manager.mutex);

    if ((memblock == NULL && size == 0) || manager.head == NULL && memblock != NULL || (memblock != NULL && pointer_valid != get_pointer_type(memblock)) || heap_validate()){
    
        pthread_mutex_unlock(&manager.mutex);
        return NULL;
    }

    if (memblock != NULL && size == 0){

        heap_free(memblock);

        pthread_mutex_unlock(&manager.mutex);
        return NULL;
    }
    
    if (memblock == NULL && size != 0){

        void * ptr = heap_malloc_aligned(size);

        pthread_mutex_unlock(&manager.mutex);
        return ptr;
    }

    if (((chunk)((intptr_t)memblock - BEF_DATA))->len >= size){

        ((chunk)((intptr_t)memblock - BEF_DATA))->len = size;
        ((chunk)((intptr_t)memblock - BEF_DATA))->checksum = checksum((void*)((intptr_t)memblock - BEF_DATA));
        *((uint64_t*)((uint8_t*)memblock + size)) = UINT64_MAX;

        if (((chunk)((intptr_t)memblock - BEF_DATA))->next == NULL && manager.end - (intptr_t)memblock - ((chunk)((uint8_t*)memblock - BEF_DATA))->len - FENCE >= PAGE_SIZE){

            custom_sbrk(-((manager.end - (intptr_t)memblock - ((chunk)((uint8_t*)memblock - BEF_DATA))->len - FENCE)/PAGE_SIZE * PAGE_SIZE));
            manager.end -= (manager.end - (intptr_t)memblock - ((chunk)((uint8_t*)memblock - BEF_DATA))->len - FENCE)/PAGE_SIZE * PAGE_SIZE;
        }

        pthread_mutex_unlock(&manager.mutex);
        return memblock;
    }

    chunk el = manager.head;

    while(el != NULL){

        if (el->start == (intptr_t)memblock){

            if (el->next == NULL){

                if ((intptr_t)(el->start + size + FENCE) > manager.end){

                    if (-1 == (intptr_t)custom_sbrk(((size - el->len - PAGE_SIZE + REMAINDER((uint8_t*)el->start + el->len))/PAGE_SIZE + 1) * PAGE_SIZE)){

                        void * alloc = heap_malloc_aligned(size);

                        if (alloc != NULL){

                            alloc = memmove(alloc, (void*)el->start, el->len);

                            heap_free((void*)el->start);

                            pthread_mutex_unlock(&manager.mutex);
                            return alloc;
                        }

                        pthread_mutex_unlock(&manager.mutex);
                        return NULL;
                    }

                    manager.end += ((size - el->len - PAGE_SIZE + REMAINDER((uint8_t*)el->start + el->len))/PAGE_SIZE + 1) * PAGE_SIZE;
                    el->len = size;
                    el->checksum = checksum(el);
                    *((uint64_t*)(el->start + size)) = UINT64_MAX;

                    pthread_mutex_unlock(&manager.mutex);
                    return (void*)el->start;
                }
                else{

                    el->len = size;
                    el->checksum = checksum((void*)el);
                    *((uint64_t*)(el->start + size)) = UINT64_MAX;

                    pthread_mutex_unlock(&manager.mutex);
                    return (void*)el->start;
                }
            }
            else{

                if ((intptr_t)(el->start + size + FENCE) > (intptr_t)el->next){

                    void * alloc = heap_malloc_aligned(size);

                    if (alloc != NULL){

                        alloc = memmove(alloc, (void*)el->start, el->len);

                        heap_free((void*)el->start);

                        pthread_mutex_unlock(&manager.mutex);
                        return alloc;
                    }

                    pthread_mutex_unlock(&manager.mutex);
                    return NULL;

                }
                else{

                    el->len = size;
                    el->checksum = checksum((void*)el);
                    *((uint64_t*)(el->start + size)) = UINT64_MAX;

                    pthread_mutex_unlock(&manager.mutex);
                    return (void*)el->start;
                }
            }
        }

        el = el->next;
    }

    pthread_mutex_unlock(&manager.mutex);
    return NULL;
}

void* heap_malloc_debug(size_t count, int fileline, const char* filename){

    pthread_mutex_lock(&manager.mutex);

    void * ptr = heap_malloc(count);
    
    if(ptr){

        ((chunk)((uint8_t*)ptr - BEF_DATA))->filename = filename;
        ((chunk)((uint8_t*)ptr - BEF_DATA))->line = fileline;
        ((chunk)((uint8_t*)ptr - BEF_DATA))->checksum = checksum((uint8_t*)ptr - BEF_DATA);
    }

    pthread_mutex_unlock(&manager.mutex);
    return ptr;
}

void* heap_calloc_debug(size_t number, size_t size, int fileline, const char* filename){

    pthread_mutex_lock(&manager.mutex);

    void * ptr = heap_malloc_debug(number*size, fileline, filename);

    if (!ptr){
    
        pthread_mutex_unlock(&manager.mutex);
        return NULL;
    }

    ptr = memset(ptr, 0, number*size);

    pthread_mutex_unlock(&manager.mutex);
    return ptr;

}

void* heap_realloc_debug(void* memblock, size_t size, int fileline, const char* filename){

    pthread_mutex_lock(&manager.mutex);

    void * ptr = heap_realloc(memblock, size);
    
    if(ptr){

        ((chunk)((uint8_t*)ptr - BEF_DATA))->filename = filename;
        ((chunk)((uint8_t*)ptr - BEF_DATA))->line = fileline;
        ((chunk)((uint8_t*)ptr - BEF_DATA))->checksum = checksum((uint8_t*)ptr - BEF_DATA);
    }

    pthread_mutex_unlock(&manager.mutex);
    return ptr;
}

void* heap_malloc_aligned_debug(size_t count, int fileline, const char* filename){

    pthread_mutex_lock(&manager.mutex);

    void * ptr = heap_malloc_aligned(count);
    
    if(ptr){

        ((chunk)((uint8_t*)ptr - BEF_DATA))->filename = filename;
        ((chunk)((uint8_t*)ptr - BEF_DATA))->line = fileline;
        ((chunk)((uint8_t*)ptr - BEF_DATA))->checksum = checksum((uint8_t*)ptr - BEF_DATA);
    }

    pthread_mutex_unlock(&manager.mutex);
    return ptr;

}

void* heap_calloc_aligned_debug(size_t number, size_t size, int fileline, const char* filename){

    pthread_mutex_lock(&manager.mutex);

    void * ptr = heap_malloc_aligned_debug(number*size, fileline, filename);

    if (!ptr){

        pthread_mutex_unlock(&manager.mutex);
        return NULL;
    }

    ptr = memset(ptr, 0, number*size);

    pthread_mutex_unlock(&manager.mutex);
    return ptr;

}

void* heap_realloc_aligned_debug(void* memblock, size_t size, int fileline, const char* filename){

    pthread_mutex_lock(&manager.mutex);

    void * ptr = heap_realloc_aligned(memblock, size);
    
    if(ptr){

        ((chunk)((uint8_t*)ptr - BEF_DATA))->filename = filename;
        ((chunk)((uint8_t*)ptr - BEF_DATA))->line = fileline;
        ((chunk)((uint8_t*)ptr - BEF_DATA))->checksum = checksum((uint8_t*)ptr - BEF_DATA);
    }

    pthread_mutex_unlock(&manager.mutex);
    return ptr;

}
