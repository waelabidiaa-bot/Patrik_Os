#ifndef VALLOC_H
#define VALLOC_H

#include <stddef.h>
#include <stdint.h>

#define VALLOC_OK           0
#define VALLOC_ERROR       -1
#define VALLOC_NO_MEMORY   -2
#define VALLOC_NOT_FOUND   -3
#define VALLOC_INVALID     -4

void valloc_initialize(void);

void *valloc(size_t size);

int vfree(void *pointer);

#endif