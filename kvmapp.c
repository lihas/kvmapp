#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
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

/** Path to KVM subsystem device file */
#define KVM_PATH "/dev/kvm"

/** Maximum number of virtual CPUs */
#define MAX_VCPUS 4

/** Virtual machine structure */
struct vm {
	int vm_fd;                       /**< virtual machine descriptor */
	unsigned num_vcpus;              /**< number of virtual CPUs */
	unsigned vcpu_mmap_size;         /**< size of shared vCPU region */
	int vcpu_fd[MAX_VCPUS];          /**< vCPU file descriptors */
	struct kvm_run *vcpu[MAX_VCPUS]; /**< vCPU kvm_run structures */
	unsigned num_mem_slots;          /**< number of attached mem slots */
};

static int  kvm_open(const char *);

static int  vm_create(int, struct vm *);
static int  vm_create_vcpu(struct vm *);
static int  vm_attach_memory(struct vm *, uintptr_t, size_t, void *);
static int  vm_get_regs(struct vm *, unsigned, struct kvm_regs *);
static int  vm_set_regs(struct vm *, unsigned, const struct kvm_regs *);
static int  vm_get_sregs(struct vm *, unsigned, struct kvm_sregs *);
static int  vm_set_sregs(struct vm *, unsigned, const struct kvm_sregs *);
static int  vm_run(struct vm *, unsigned);
static void vm_destroy(struct vm *);

/**
 * Obtain a handle to KVM subsystem
 *
 * @param path path to KVM subsystem device file
 *
 * @return KVM subsystem handle, or -1 if an error occured
 */
int kvm_open(const char *path)
{
	int fd;

	assert(path != NULL);

	fd = open(path, O_RDWR);
	if (fd > 0 && ioctl(fd, KVM_GET_API_VERSION, 0) != KVM_API_VERSION) {
		close(fd);
		fd = -1;
	}

	return fd;
}

/**
 * Create a virtual machine
 *
 * @param kvm KVM subsystem handle
 * @param vm  virtual machine descriptor
 *
 * @return zero on success, or -1 if an error occured
 */
int vm_create(int kvm, struct vm *vm)
{
	int ret = -1;

	assert(kvm > 0);
	assert(vm != NULL);

	memset(vm, 0, sizeof(*vm));

	vm->vcpu_mmap_size = ioctl(kvm, KVM_GET_VCPU_MMAP_SIZE, 0);
	if (vm->vcpu_mmap_size > 0) {
		vm->vm_fd = ioctl(kvm, KVM_CREATE_VM, 0);
		if (vm->vm_fd > 0)
			ret = 0;
		else
			vm_destroy(vm);
	}

	return ret;
}

/**
 * Create a new virtual CPU for a virtual machine
 *
 * @param vm virtual machine descriptor
 *
 * @return ID of created virtual CPU, or -1 if an error occured
 */
int vm_create_vcpu(struct vm *vm)
{
	int ret = -1;
	int i;

	assert(vm != NULL);
	assert(vm->vm_fd > 0);

	i = vm->num_vcpus;
	if (i < MAX_VCPUS) {
		vm->vcpu_fd[i] = ioctl(vm->vm_fd, KVM_CREATE_VCPU, i);
		if (vm->vcpu_fd[i] > 0) {
			vm->vcpu[i] = mmap(0, vm->vcpu_mmap_size,
					   PROT_READ | PROT_WRITE,
					   MAP_PRIVATE, vm->vcpu_fd[i], 0);
			if (vm->vcpu[i] != NULL)
				ret = vm->num_vcpus++;
			else {
				close(vm->vcpu_fd[i]);
				vm->vcpu_fd[i] = 0;
			}
		}
	}

	return ret;
}

/**
 * Attach a memory region to a virtual machine
 *
 * @param vm   virtual machine descriptor
 * @param gpa  guest physical address
 * @param size memory region size
 * @param addr start of host addressable memory region
 *
 * @return ID of created memory region, or -1 if an error occured
 */
int vm_attach_memory(struct vm *vm, uintptr_t gpa, size_t size, void *addr)
{
	struct kvm_userspace_memory_region mem;

	assert(vm != NULL);
	assert(vm->vm_fd > 0);
	assert(size > 0 && size % PAGE_SIZE == 0);
	assert(addr != NULL);

	mem.slot = vm->num_mem_slots;
	mem.flags = 0; /* read/write */
	mem.guest_phys_addr = gpa;
	mem.memory_size = size;
	mem.userspace_addr = (uintptr_t) addr;

	if (ioctl(vm->vm_fd, KVM_SET_USER_MEMORY_REGION, &mem) == 0)
		return vm->num_mem_slots++;

	return -1;
}

/**
 * Read general purpose registers from a virtual CPU
 *
 * @param vm   virtual machine descriptor
 * @param vcpu virtual CPU identifier
 * @param regs general purpose registers
 *
 * @return zero on success, or -1 if an error occured
 */
int vm_get_regs(struct vm *vm, unsigned vcpu, struct kvm_regs *regs)
{
	assert(vm != NULL);
	assert(vm->num_vcpus > vcpu);
	assert(vm->vcpu_fd[vcpu] > 0);
	assert(regs != NULL);

	return ioctl(vm->vcpu_fd[vcpu], KVM_GET_REGS, regs);
}

/**
 * Write general purpose registers into a virtual CPU
 *
 * @param vm   virtual machine descriptor
 * @param vcpu virtual CPU identifier
 * @param regs general purpose registers
 *
 * @return zero on success, or -1 if an error occured
 */
int vm_set_regs(struct vm *vm, unsigned vcpu, const struct kvm_regs *regs)
{
	assert(vm != NULL);
	assert(vm->num_vcpus > vcpu);
	assert(vm->vcpu_fd[vcpu] > 0);
	assert(regs != NULL);

	return ioctl(vm->vcpu_fd[vcpu], KVM_SET_REGS, regs);
}

/**
 * Read special registers from a virtual CPU
 *
 * @param vm   virtual machine descriptor
 * @param vcpu virtual CPU identifier
 * @param regs special registers
 *
 * @return zero on success, or -1 if an error occured
 */
int vm_get_sregs(struct vm *vm, unsigned vcpu, struct kvm_sregs *regs)
{
	assert(vm != NULL);
	assert(vm->num_vcpus > vcpu);
	assert(vm->vcpu_fd[vcpu] > 0);
	assert(regs != NULL);

	return ioctl(vm->vcpu_fd[vcpu], KVM_GET_SREGS, regs);
}

/**
 * Write special registers into a virtual CPU
 *
 * @param vm   virtual machine descriptor
 * @param vcpu virtual CPU identifier
 * @param regs special registers
 *
 * @return zero on success, or -1 if an error occured
 */
int vm_set_sregs(struct vm *vm, unsigned vcpu, const struct kvm_sregs *regs)
{
	assert(vm != NULL);
	assert(vm->num_vcpus > vcpu);
	assert(vm->vcpu_fd[vcpu] > 0);
	assert(regs != NULL);

	return ioctl(vm->vcpu_fd[vcpu], KVM_SET_SREGS, regs);
}

/**
 * Run a virtual CPU of a virtual machine
 *
 * @param vm   virtual machine descriptor
 * @param vcpu virtua CPU identifier
 *
 * @return zero on success, or -1 if an error occured
 */
int vm_run(struct vm *vm, unsigned vcpu)
{
	assert(vm != NULL);
	assert(vm->num_vcpus > vcpu);
	assert(vm->vcpu_fd[vcpu] > 0);

	return ioctl(vm->vcpu_fd[vcpu], KVM_RUN, 0);
}

/**
 * Destroy a virtual machine
 *
 * @param vm virtual machine descriptor
 */
void vm_destroy(struct vm *vm)
{
	int i;

	assert(vm != NULL);

	for (i = 0; i < vm->num_vcpus; i++) {
		if (vm->vcpu_fd[i] > 0)
			close(vm->vcpu_fd[i]);
		if (vm->vcpu[i] != NULL)
			munmap(vm->vcpu[i], vm->vcpu_mmap_size);
	}

	if (vm->vm_fd > 0)
		close(vm->vm_fd);
}

/* #define UNRESTRICTED_GUEST */
#define PROTECTED_GUEST
#define PAGED_GUEST

#ifdef UNRESTRICTED_GUEST
# include "unrestricted_guest.bin.h"
#elif defined(PROTECTED_GUEST)
# include "protected_guest.bin.h"
#endif

int main(int argc, const char *argv[])
{
	static const size_t num_bytes = 0x100000;

	int rc;
	int kvm;
	struct vm vm;
	void *guestmem;
	struct kvm_regs regs;
	int ret = EXIT_FAILURE;

	kvm = kvm_open(KVM_PATH);
	if (kvm < 0)
		goto out;

	if (vm_create(kvm, &vm) != 0)
		goto out_kvm;

	if (vm_create_vcpu(&vm) < 0)
		goto out_vm;

	guestmem = mmap(0, num_bytes, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (guestmem == MAP_FAILED)
		goto out_vm;

	if (vm_attach_memory(&vm, 0x0, num_bytes, guestmem) < 0)
		goto out_guestmem;

	if (vm_get_regs(&vm, 0, &regs) != 0)
		goto out_guestmem;

	regs.rflags = 0x2;
	regs.rip = 0x0;
	if (vm_set_regs(&vm, 0, &regs) != 0)
		goto out_guestmem;

#ifdef UNRESTRICTED_GUEST
	memcpy(guestmem, unrestricted_guest_bin, unrestricted_guest_bin_len);
#else /* UNRESTRICTED_GUEST */
	struct kvm_sregs sregs;

	memcpy(guestmem, protected_guest_bin, protected_guest_bin_len);

	if (vm_get_sregs(&vm, 0, &sregs) != 0)
		goto out_guestmem;

	sregs.cs.base  = sregs.ss.base  = sregs.ds.base  = 0x0;
	sregs.cs.limit = sregs.ss.limit = sregs.ds.limit = 0xffffffff;
	sregs.cs.g     = sregs.ss.g     = sregs.ds.g     = 1;
	sregs.cs.db    = sregs.ss.db                     = 1;

	sregs.cr0 |= 1; /* enabled protected mode */

#ifdef PAGED_GUEST
	sregs.cr0 |= 1UL << 31; /* enable paged mode */
	sregs.cr4 |= 1UL << 4;  /* enable page size extension */
	sregs.cr3 = 0x1000;

	uint32_t *pdir = guestmem + 0x1000;
	for (int i = 0; i < 1024; i++)
		pdir[i] = (i << 22) | 0x87;
#endif

	if (vm_set_sregs(&vm, 0, &sregs) != 0)
		goto out_guestmem;
#endif

	for (/* NOTHING */; /* NOTHING */; /* NOTHING */) {
		struct kvm_run *vcpu = vm.vcpu[0];

		rc = vm_run(&vm, 0);
		if (rc != 0)
			break;

		if (vcpu->exit_reason == KVM_EXIT_HLT)
			break;

		if (vcpu->exit_reason == KVM_EXIT_IO &&
		    vcpu->io.port == 0x3f8 &&
		    vcpu->io.direction == KVM_EXIT_IO_OUT)
		{
			write(STDOUT_FILENO,
			      (const void *) vcpu + vcpu->io.data_offset,
			      vcpu->io.size * vcpu->io.count);
		}
	}

	ret = EXIT_SUCCESS;

out_guestmem:
	munmap(guestmem, num_bytes);

out_vm:
	vm_destroy(&vm);

out_kvm:
	close(kvm);

out:
	return ret;
}
