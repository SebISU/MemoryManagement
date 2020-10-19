#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "heap.h"

int main(void) {
/*
    srand (time(NULL));

    int status = heap_setup();

    char *ptr[20371];
    int ptr_state[20371] = {0};

    int is_allocated = 0;

    for (int i = 0; i < 20371; ++i)
    {
        int rand_value = rand() % 100;
        if (rand_value < 33)
        {
            for (int j = 0; j < 20371; ++j)
                if (ptr_state[j] == 0)
                {
                    ptr_state[j] = 1;
                    ptr[j] = heap_realloc(NULL, rand() % 1000 + 500);
                    is_allocated++;
                    break;
                }
            }
        else if (rand_value < 66)
        {
        if (is_allocated)
            {
                int to_reallocate = rand() % is_allocated;
                for (int j = 0; j < 20371; ++j)
                {
                    if (ptr_state[j] == 1 && !to_reallocate)
                    {
                        if (rand() % 100 < 50)
                            ptr[j] = heap_realloc(ptr[j], rand() % 1000 + 500);
                        else
                            ptr[j] = heap_realloc(ptr[j], rand() % 10000 + 5000);  
                        break;
                    }
                    to_reallocate--;
                }
            }
        }
        else
        {
            if (is_allocated)
            {
                int to_free = rand() % is_allocated;
                for (int j = 0; j < 20371; ++j)
                {
                    if (ptr_state[j] == 1 && !to_free)
                    {
                        ptr_state[j] = 0;
                        is_allocated--;
                        heap_realloc(ptr[j], 0);
                        break;
                    }
                    to_free--;
                }
            }
        }
        status = heap_validate();

        if (status)
            printf("%d ", status);
    }

    for (int j = 0; j < 20371; ++j)
        if (ptr_state[j] == 1)
            heap_realloc(ptr[j], 0);
            */

    return 0;
}