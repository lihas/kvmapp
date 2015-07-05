#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/user.h>
#include <unistd.h>

#include <linux/kvm.h>

#include "kvm.h"
#include "log.h"

/**
 * enum
 *
 * @MAX_VCPUS:    maximum number of virtual CPUs
 * @MAX_MEMSLOTS: maximum number of memory slots
 */
enum {
	MAX_VCPUS    = 4,
	MAX_MEMSLOTS = 8,
};

/**
 * struct vm - virtual machine structure
 *
 * @vm_fd:          virtual machine file descriptor
 * @num_vcpus:      number of virtual CPUs
 * @vcpu_mmap_size: size of shared virtual CPU region
 * @vcpu_fd:        virtual CPU file descriptors
 * @vcpu:           mmaped virtual CPU shared regions
 * @num_mem_slots:  number of attached memory slots
 */
struct vm {
	int vm_fd;
	unsigned num_vcpus;
	unsigned vcpu_mmap_size;
	int vcpu_fd[MAX_VCPUS];
	struct kvm_run *vcpu[MAX_VCPUS];
	unsigned num_mem_slots;
	struct kvm_userspace_memory_region mem_slot[MAX_MEMSLOTS];
};

/**
 * kvm_open() - obtain a handle to KVM subsystem
 *
 * @path: path to KVM subsystem device file
 *
 * Return: KVM subsystem handle, or -1 if an error occured
 */
int kvm_open(const char *path)
{
	int fd;

	assert(path != NULL);

	fd = open(path, O_RDWR);
	if (fd < 0) {
		error("%s", path);
		return -1;
	}

	if (ioctl(fd, KVM_GET_API_VERSION, 0) != KVM_API_VERSION) {
		errorx("unsupported KVM API version number");
		close(fd);
		return -1;
	}

	return fd;
}

/**
 * kvm_close() - close a handle to KVM subsystem
 *
 * @kvm: KVM subsystem handle
 */
void kvm_close(int kvm)
{
	assert(kvm > 0);

	close(kvm);
}

/**
 * vm_create() - create a virtual machine
 *
 * @kvm: KVM subsystem handle
 *
 * Return: virtual machine descriptor, or NULL if an error occured
 */
struct vm *vm_create(int kvm)
{
	struct vm *vm;

	assert(kvm > 0);

	vm = malloc(sizeof(*vm));
	if (vm == NULL) {
		error("failed to allocate virtual machine structure");
		return NULL;
	}

	memset(vm, 0, sizeof(*vm));
	vm->vcpu_mmap_size = ioctl(kvm, KVM_GET_VCPU_MMAP_SIZE, 0);
	vm->vm_fd = ioctl(kvm, KVM_CREATE_VM, 0);
	if (vm->vm_fd < 0) {
		error("failed to create virtual machine");
		free(vm);
		return NULL;
	}

	return vm;
}

/**
 * vm_attach_memory() - attach a memory region to a virtual machine
 *
 * @vm:   virtual machine descriptor
 * @gpa:  guest physical address
 * @size: memory region size
 * @addr: start of host addressable memory region
 *
 * Return: ID of created memory region, or -1 if an error occured
 */
int vm_attach_memory(struct vm *vm, uintptr_t gpa, size_t size, void *addr)
{
	struct kvm_userspace_memory_region *mem;

	assert(vm != NULL);
	assert(vm->vm_fd > 0);
	assert(size > 0 && size % PAGE_SIZE == 0);
	assert(addr != NULL);

	if (vm->num_mem_slots >= MAX_MEMSLOTS) {
		errorx("out of free memory regions");
		return -1;
	}

	mem = &vm->mem_slot[vm->num_mem_slots];
	mem->slot = vm->num_mem_slots;
	mem->flags = 0; /* read/write */
	mem->guest_phys_addr = gpa;
	mem->memory_size = size;
	mem->userspace_addr = (uintptr_t) addr;

	if (ioctl(vm->vm_fd, KVM_SET_USER_MEMORY_REGION, mem) != 0) {
		error("failed to set user memory region #%u", mem->slot);
		return -1;
	}

	return vm->num_mem_slots++;
}

/**
 * vm_get_memory() - get host addressable memory region from a virtual machine
 *
 * @vm:   virtual machine descriptor
 * @gpa:  guest physical address
 * @size: memory region size
 *
 * Return: start of host addressable memory region, or NULL on failure
 */
void *vm_get_memory(struct vm *vm, uintptr_t gpa, size_t size)
{
	struct kvm_userspace_memory_region *m;

	assert(vm != NULL);

	for (m = vm->mem_slot; m < vm->mem_slot + vm->num_mem_slots; m++)
		if (m->guest_phys_addr <= gpa &&
		    m->guest_phys_addr + m->memory_size >= gpa + size)
			return (void *) m->userspace_addr +
			    (gpa - m->guest_phys_addr);

	errorx("no memory region found for 0x%" PRIxPTR "..0x%" PRIxPTR,
	       gpa, gpa + size);

	return NULL;
}

/**
 * vm_destroy() - destroy a virtual machine
 *
 * @vm: virtual machine descriptor
 */
void vm_destroy(struct vm *vm)
{
	unsigned i;

	assert(vm != NULL);

	for (i = 0; i < vm->num_vcpus; i++) {
		if (vm->vcpu_fd[i] > 0)
			close(vm->vcpu_fd[i]);
		if (vm->vcpu[i] != NULL)
			munmap(vm->vcpu[i], vm->vcpu_mmap_size);
	}

	if (vm->vm_fd > 0)
		close(vm->vm_fd);

	free(vm);
}

/**
 * vcpu_create() - create a new virtual CPU for a virtual machine
 *
 * @vm: virtual machine descriptor
 *
 * Return: ID of created virtual CPU, or -1 if an error occured
 */
int vcpu_create(struct vm *vm)
{
	unsigned i;

	assert(vm != NULL);
	assert(vm->vm_fd > 0);

	i = vm->num_vcpus;
	if (i >= MAX_VCPUS) {
		errorx("out of free virtual CPUs");
		return -1;
	}

	vm->vcpu_fd[i] = ioctl(vm->vm_fd, KVM_CREATE_VCPU, i);
	if (vm->vcpu_fd[i] < 0) {
		error("failed to create VCPU #%u", i);
		return -1;
	}

	vm->vcpu[i] = mmap(0, vm->vcpu_mmap_size, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE, vm->vcpu_fd[i], 0);
	if (vm->vcpu[i] == NULL) {
		error("failed to map VCPU #%u", i);
		close(vm->vcpu_fd[i]);
		vm->vcpu_fd[i] = 0;
		return -1;
	}

	return vm->num_vcpus++;
}

/**
 * vcpu_get_regs() - read general purpose registers from a virtual CPU
 *
 * @vm:   virtual machine descriptor
 * @vcpu: virtual CPU identifier
 * @regs: general purpose registers
 *
 * Return: zero on success, or -1 if an error occured
 */
int vcpu_get_regs(struct vm *vm, unsigned vcpu, struct kvm_regs *regs)
{
	int ret;

	assert(vm != NULL);
	assert(vm->num_vcpus > vcpu);
	assert(vm->vcpu_fd[vcpu] > 0);
	assert(regs != NULL);

	ret = ioctl(vm->vcpu_fd[vcpu], KVM_GET_REGS, regs);
	if (ret != 0)
		error("failed to get VCPU #%u registers", vcpu);

	return ret;
}

/**
 * vcpu_set_regs() - write general purpose registers into a virtual CPU
 *
 * @vm:   virtual machine descriptor
 * @vcpu: virtual CPU identifier
 * @regs: general purpose registers
 *
 * Return: zero on success, or -1 if an error occured
 */
int vcpu_set_regs(struct vm *vm, unsigned vcpu, const struct kvm_regs *regs)
{
	int ret;

	assert(vm != NULL);
	assert(vm->num_vcpus > vcpu);
	assert(vm->vcpu_fd[vcpu] > 0);
	assert(regs != NULL);

	ret = ioctl(vm->vcpu_fd[vcpu], KVM_SET_REGS, regs);
	if (ret != 0)
		error("failed to set VCPU #%u registers", vcpu);

	return ret;
}

/**
 * vcpu_get_sregs() - read special registers from a virtual CPU
 *
 * @vm:   virtual machine descriptor
 * @vcpu: virtual CPU identifier
 * @regs: special registers
 *
 * Return: zero on success, or -1 if an error occured
 */
int vcpu_get_sregs(struct vm *vm, unsigned vcpu, struct kvm_sregs *regs)
{
	int ret;

	assert(vm != NULL);
	assert(vm->num_vcpus > vcpu);
	assert(vm->vcpu_fd[vcpu] > 0);
	assert(regs != NULL);

	ret = ioctl(vm->vcpu_fd[vcpu], KVM_GET_SREGS, regs);
	if (ret != 0)
		error("failed to get VCPU #%u special registers", vcpu);

	return ret;
}

/**
 * vcpu_set_sregs() - write special registers into a virtual CPU
 *
 * @vm:   virtual machine descriptor
 * @vcpu: virtual CPU identifier
 * @regs: special registers
 *
 * Return: zero on success, or -1 if an error occured
 */
int vcpu_set_sregs(struct vm *vm, unsigned vcpu, const struct kvm_sregs *regs)
{
	int ret;

	assert(vm != NULL);
	assert(vm->num_vcpus > vcpu);
	assert(vm->vcpu_fd[vcpu] > 0);
	assert(regs != NULL);

	ret = ioctl(vm->vcpu_fd[vcpu], KVM_SET_SREGS, regs);
	if (ret != 0)
		error("failed to set VCPU #%u special registers", vcpu);

	return ret;
}

/**
 * vcpu_get() - get virtual CPU parameter block
 *
 * @vm: virtual machine descriptor
 * @vcpu: virtual CPU indentifier
 *
 * Return: pointer to virtual CPU parameter block
 */
struct kvm_run *vcpu_get(struct vm *vm, unsigned vcpu)
{
	assert(vm != NULL);
	assert(vm->num_vcpus > vcpu);
	assert(vm->vcpu[vcpu] != NULL);

	return vm->vcpu[vcpu];
}

/**
 * vcpu_run() - run a virtual CPU of a virtual machine
 *
 * @vm:   virtual machine descriptor
 * @vcpu: virtua CPU identifier
 *
 * Return: zero on success, or -1 if an error occured
 */
int vcpu_run(struct vm *vm, unsigned vcpu)
{
	int ret;

	assert(vm != NULL);
	assert(vm->num_vcpus > vcpu);
	assert(vm->vcpu_fd[vcpu] > 0);

	ret = ioctl(vm->vcpu_fd[vcpu], KVM_RUN, 0);
	if (ret != 0)
		error("failed to run VCPU #%u", vcpu);

	return ret;
}
