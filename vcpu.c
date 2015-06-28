#include <sys/user.h>

#include <linux/kvm.h>

#include "kvm.h"
#include "vcpu.h"

/**
 * vcpu_init() - perform common initialization of a virtual CPU
 *
 * @vm:    virtual machine descriptor
 * @vcpu:  ID of a virtual CPU to initialize
 * @entry: guest physical entry point (RIP value)
 *
 * Return: zero on success, or -1 if an error occurred
 */
int vcpu_init(struct vm *vm, unsigned vcpu, uintptr_t entry)
{
	struct kvm_regs regs;

	if (vcpu_get_regs(vm, vcpu, &regs) != 0)
		return -1;

	regs.rflags = 0x2;
	regs.rip = entry;

	if (vcpu_set_regs(vm, vcpu, &regs) != 0)
		return -1;

	return 0;
}

/**
 * vcpu_enable_protected_mode() - enable protected mode on a virtual CPU
 *
 * @vm:   virtual machine descriptor
 * @vcpu: ID of a virtual CPU to enable protected mode on
 *
 * Return: zero on success, or -1 if an error occurred
 */
int vcpu_enable_protected_mode(struct vm *vm, unsigned vcpu)
{
	struct kvm_sregs sregs;

	if (vcpu_get_sregs(vm, vcpu, &sregs) != 0)
		return -1;

	sregs.cs.base  = sregs.ss.base  = sregs.ds.base  = 0x0;
	sregs.cs.limit = sregs.ss.limit = sregs.ds.limit = 0xffffffff;
	sregs.cs.g     = sregs.ss.g     = sregs.ds.g     = 1;
	sregs.cs.db    = sregs.ss.db                     = 1;

	sregs.cr0 |= 1; /* enable protected mode */

	if (vcpu_set_sregs(vm, vcpu, &sregs) != 0)
		return -1;

	return 0;
}

/**
 * vcpu_enable_paged_mode() - enabled paged mode on a virtual CPU
 *
 * @vm:   virtual machine descriptor
 * @vcpu: ID of a virtual CPU to enable paged mode on
 * @pdir: guest physical address for an identity page directory
 *
 * Return: zero on success, or -1 if an error occurred
 */
int vcpu_enable_paged_mode(struct vm *vm, unsigned vcpu, uintptr_t pdir)
{
	struct kvm_sregs sregs;
	uint32_t *pd;
	int i;

	if (vcpu_get_sregs(vm, vcpu, &sregs) != 0)
		return -1;

	sregs.cr0 |= 1UL << 31; /* enable paged mode */
	sregs.cr4 |= 1UL << 4;  /* enable page size extension */
	sregs.cr3 = pdir;

	pd = vm_get_memory(vm, pdir, PAGE_SIZE);
	if (pd == NULL)
		return -1;

	for (i = 0; i < 1024; i++)
		pd[i] = (i << 22) | 0x87;

	if (vcpu_set_sregs(vm, vcpu, &sregs) != 0)
		return -1;

	return 0;
}
