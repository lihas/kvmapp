#include <stdio.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <unistd.h>

#include <linux/kvm.h>

#include "kvm.h"
#include "loader/binary.h"
#include "log.h"

#define KVM_PATH  "/dev/kvm" /* default path to KVM subsystem device file */
#define NUM_BYTES 0x100000   /* default guest memory size                 */

/**
 * uasge() - print usage information to supplied output stream and exit
 *
 * @stream: output stream
 */
static void NORETURN usage(const char *progname, FILE *stream)
{
	fprintf(stream, "Usage: %s [-h] [-k KVM_PATH] [-m MEGABYTES] IMAGE\n",
		progname);

	exit(stream == stdout ? EXIT_SUCCESS : EXIT_FAILURE);
	/* NOTREACHED */
}

/**
 * start_vm() - initialize and start a virtual machine
 *
 * @kvm_path:   path to KVM subsystem device file
 * @image_path: path to guest image file
 * @num_bytes:  size of guest memory in bytes
 *
 * Return: exit code value
 */
static int start_vm(const char *kvm_path, const char *image_path,
		    size_t num_bytes)
{
	int rc, kvm;
	struct vm *vm;
	void *guestmem;
	struct kvm_run *vcpu;
	int ret = EXIT_FAILURE;

	kvm = kvm_open(kvm_path);
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

	rc = binary_load(vm, image_path, 0,
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

int main(int argc, char *argv[])
{
	const char *kvm_path = KVM_PATH;
	const char *image_path = NULL;
	size_t num_bytes = NUM_BYTES;
	char *num_bytes_endptr;
	int opt;

	while ((opt = getopt(argc, argv, "k:m:h")) != -1)
		switch (opt) {
		case 'k':
			kvm_path = optarg;
			break;
		case 'm':
			num_bytes = strtol(optarg, &num_bytes_endptr, 10);
			if (*num_bytes_endptr != '\0') {
				errorx("%s: wrong guest memory size", optarg);
				usage(argv[0], stderr);
				/* NOTREACHED */
			}
			num_bytes <<= 20;
			break;
		case 'h':
			/* FALLTHROUGH */
		default:
			usage(argv[0], opt == 'h' ? stdout : stderr);
			/* NOTREACHED */
		}

	if (argc - optind != 1) {
		errorx("missing image file name");
		usage(argv[0], stderr);
		/* NOTREACHED */
	}

	image_path = argv[optind];

	return start_vm(kvm_path, image_path, num_bytes);
}
