#ifndef _VCPU_H
#define _VCPU_H

#include <stdint.h>

struct vm;

/**
 * enum
 *
 * @BOOT_VCPU: default bootstrap virtual CPU ID
 */
enum {
	BOOT_VCPU = 0,
};

int vcpu_init(struct vm *, unsigned, uintptr_t, uintptr_t);
int vcpu_enable_protected_mode(struct vm *, unsigned);
int vcpu_enable_paged_mode(struct vm *, unsigned, uintptr_t);

#endif /* _VCPU_H */
