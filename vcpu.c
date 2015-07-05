#include <sys/user.h>

#include <linux/kvm.h>

#include "kvm.h"
#include "log.h"
#include "vcpu.h"

/**
 * enum
 *
 * @CR0_PE:  protected mode enable
 * @CR0_PG:  paging mode enable
 * @CR4_PSE: page size extension enable
 *
 * @PDE_P:   page directory entry present bit
 * @PDE_RW:  page directory entry read/write bit
 * @PDE_S:   page directory entry supervisor bit
 * @PDE_PS:  page directory entry page size bit (4MB)
 * @PDE_RWP: same as PDE_RW | PDE_P
 */
enum {
	CR0_PE  = 1UL << 0,
	CR0_PG  = 1UL << 31,
	CR4_PSE = 1UL << 4,

	PDE_P   = 1UL << 0,
	PDE_RW  = 1UL << 1,
	PDE_S   = 1UL << 2,
	PDE_PS  = 1UL << 7,

	PDE_RWP = PDE_RW | PDE_P,
};

/**
 * vcpu_init() - perform common initialization of a virtual CPU
 *
 * @vm:    virtual machine descriptor
 * @vcpu:  ID of a virtual CPU to initialize
 * @entry: guest physical entry point (RIP value)
 * @stack: guest physical stack top (RSP value)
 *
 * Return: zero on success, or -1 if an error occurred
 */
int vcpu_init(struct vm *vm, unsigned vcpu, uintptr_t entry, uintptr_t stack)
{
	struct kvm_regs regs;

	if (vcpu_get_regs(vm, vcpu, &regs) == 0) {
		regs.rflags = 0x2;
		regs.rip = entry;
		regs.rsp = stack;

		if (vcpu_set_regs(vm, vcpu, &regs) == 0)
			return 0;
	}

	errorx("failed to initiazle VCPU #%u", vcpu);

	return -1;
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

	if (vcpu_get_sregs(vm, vcpu, &sregs) == 0) {
		sregs.cs.base  = sregs.ss.base  = sregs.ds.base  = 0x0;
		sregs.cs.limit = sregs.ss.limit = sregs.ds.limit = 0xffffffff;
		sregs.cs.g     = sregs.ss.g     = sregs.ds.g     = 1;
		sregs.cs.db    = sregs.ss.db                     = 1;

		sregs.cr0 |= CR0_PE;

		if (vcpu_set_sregs(vm, vcpu, &sregs) == 0)
			return 0;
	}

	errorx("failed to enable protected mode on VCPU #%u", vcpu);

	return -1;
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

	if (vcpu_get_sregs(vm, vcpu, &sregs) == 0) {
		sregs.cr0 |= CR0_PG;
		sregs.cr4 |= CR4_PSE;
		sregs.cr3 = pdir;

		pd = vm_get_memory(vm, pdir, PAGE_SIZE);
		if (pd != NULL) {
			/* Initialize identity mapping */
			for (i = 0; i < 1024; i++)
				pd[i] = (i << 22) | PDE_PS | PDE_S | PDE_RWP;

			if (vcpu_set_sregs(vm, vcpu, &sregs) == 0)
				return 0;
		}
	}

	errorx("failed to enable paging mode on VCPU #%u", vcpu);

	return -1;
}
