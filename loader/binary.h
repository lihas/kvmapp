#ifndef _LOADER_BINARY_H
#define _LOADER_BINARY_H

#include <stdint.h>

struct vm;

/**
 * enum - binary loader flags
 *
 * @BINARY_LOAD_UNRESTRICTED: load virtual machine in unrestricted mode
 * @BINARY_LOAD_PROTECTED:    load virtual machine in protected mode
 * @BINARY_LOAD_PAGED:        load virtual machine in paged mode
 */
enum {
	BINARY_LOAD_UNRESTRICTED = 0,
	BINARY_LOAD_PROTECTED    = 1,
	BINARY_LOAD_PAGED        = 2,
};

int binary_load(struct vm *, const char *, uintptr_t, int);

#endif /* _LOADER_BINARY_H */
