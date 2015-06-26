#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/kvm.h>

#include "vm.h"

#define KVM_PATH "/dev/kvm" /* default path to KVM subsystem device file */

static int kvm_open(const char *);

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
	if (fd > 0 && ioctl(fd, KVM_GET_API_VERSION, 0) != KVM_API_VERSION) {
		close(fd);
		fd = -1;
	}

	return fd;
}

/* #define UNRESTRICTED_GUEST */
#define PROTECTED_GUEST
#define PAGED_GUEST

#ifdef UNRESTRICTED_GUEST
# include "guest/unrestricted_guest.bin.h"
#elif defined(PROTECTED_GUEST)
# include "guest/protected_guest.bin.h"
#endif

int main(int argc, const char *argv[])
{
	static const size_t num_bytes = 0x100000;

	int i, rc, kvm;
	struct vm *vm;
	void *guestmem;
	struct kvm_regs regs;
	int ret = EXIT_FAILURE;

	(void) argc;
	(void) argv;

	kvm = kvm_open(KVM_PATH);
	if (kvm < 0)
		goto out;

	vm = vm_create(kvm);
	if (vm == NULL)
		goto out_kvm;

	if (vm_create_vcpu(vm) < 0)
		goto out_vm;

	guestmem = mmap(0, num_bytes, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (guestmem == MAP_FAILED)
		goto out_vm;

	if (vm_attach_memory(vm, 0x0, num_bytes, guestmem) < 0)
		goto out_guestmem;

	if (vm_get_regs(vm, 0, &regs) != 0)
		goto out_guestmem;

	regs.rflags = 0x2;
	regs.rip = 0x0;
	if (vm_set_regs(vm, 0, &regs) != 0)
		goto out_guestmem;

#ifdef UNRESTRICTED_GUEST
	memcpy(guestmem, guest_unrestricted_guest_bin, guest_unrestricted_guest_bin_len);
#else /* UNRESTRICTED_GUEST */
	struct kvm_sregs sregs;

	memcpy(guestmem, guest_protected_guest_bin, guest_protected_guest_bin_len);

	if (vm_get_sregs(vm, 0, &sregs) != 0)
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
	for (i = 0; i < 1024; i++)
		pdir[i] = (i << 22) | 0x87;
#else
	(void) i;
#endif

	if (vm_set_sregs(vm, 0, &sregs) != 0)
		goto out_guestmem;
#endif

	for (/* NOTHING */; /* NOTHING */; /* NOTHING */) {
		struct kvm_run *vcpu = vm_get_vcpu(vm, 0);

		rc = vm_run(vm, 0);
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
	vm_destroy(vm);

out_kvm:
	close(kvm);

out:
	return ret;
}
