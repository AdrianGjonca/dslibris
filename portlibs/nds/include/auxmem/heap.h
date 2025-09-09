#ifndef AUXMEM_HEAP_H
#define AUXMEM_HEAP_H

#include <auxmem.h>
#include <stdlib.h>
#include <stdbool.h>

extern void AMALLOC_init();

extern AM_addr_t AMALLOC_alloc(size_t size);
extern void AMALLOC_free(size_t address);
extern void AMALLOC_clean();

extern size_t AMALLOC_getfree();
extern bool AMALLOC_isempty();

#endif
