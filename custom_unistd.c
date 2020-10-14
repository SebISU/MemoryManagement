#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "custom_unistd.h"

#define PAGE_SIZE       4096    // Długość strony w bajtach
#define PAGES_AVAILABLE 16384   // Liczba stron dostępnych dla sterty

uint8_t memory[PAGE_SIZE * PAGES_AVAILABLE] __attribute__((aligned(PAGE_SIZE)));


struct mm_struct mm = {.start_brk = (intptr_t)(memory), .brk = (intptr_t)(memory)};


void* custom_sbrk(intptr_t delta)
{
    intptr_t current_brk = mm.brk;
    if (mm.brk + delta < mm.start_brk) {
        errno = 0;
        return (void*)current_brk;
    }

    mm.brk += delta;
    return (void*)current_brk;
}
