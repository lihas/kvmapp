/* #include <assert.h> */
#include <stdlib.h>
/* #include <string.h> */

/* #include <fcntl.h> */
/* #include <sys/ioctl.h> */
#include <sys/mman.h>
/* #include <sys/stat.h> */
/* #include <sys/types.h> */
#include <unistd.h>

#include <linux/kvm.h>

#include "loader/binary.h"
#include "kvm.h"

#define KVM_PATH "/dev/kvm" /* default path to KVM subsystem device file */

int main(int argc, const char *argv[])
{
	static const size_t num_bytes = 0x100000;

	int rc, kvm;
	struct vm *vm;
	void *guestmem;
	struct kvm_run *vcpu;
	int ret = EXIT_FAILURE;

	(void) argc;
	(void) argv;

	kvm = kvm_open(KVM_PATH);
	if (kvm < 0)
		goto out;

	vm = vm_create(kvm);
	if (vm == NULL)
		goto out_kvm;

	if (vcpu_create(vm) < 0)
		goto out_vm;

	guestmem = mmap(0, num_bytes, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (guestmem == MAP_FAILED)
		goto out_vm;

	if (vm_attach_memory(vm, 0x0, num_bytes, guestmem) < 0)
		goto out_guestmem;

	rc = binary_load(vm, "guest/protected_guest.bin", 0,
			 BINARY_LOAD_PROTECTED | BINARY_LOAD_PAGED);
	if (rc != 0)
		goto out_guestmem;

	vcpu = vcpu_get(vm, 0);
	for (/* NOTHING */; /* NOTHING */; /* NOTHING */) {
		rc = vcpu_run(vm, 0);
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
	kvm_close(kvm);

out:
	return ret;
}
