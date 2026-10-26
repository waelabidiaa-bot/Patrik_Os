#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>

void heap_initialize(void);
void *kmalloc(size_t size);
void kfree(void *pointer);

#endif
