#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <unistd.h>

#include <linux/kvm.h>

#include "kvm.h"
#include "loader/binary.h"
#include "log.h"
#include "vcpu.h"

#define DEFAULT_KVM_PATH   "/dev/kvm" /* default path to KVM device file */
#define DEFAULT_IMAGE_PATH NULL       /* default guest image file path   */
#define DEFAULT_NUM_BYTES  0x100000   /* default guest memory size       */

/**
 * struct config - parsed command line arguments
 *
 * @kvm_path:   path to KVM subsystem device file
 * @image_path: guest image file path
 * @num_bytes:  guest memory size in bytes
 */
struct config {
	const char *kvm_path;
	const char *image_path;
	size_t num_bytes;
};

/**
 * uasge() - print usage information to supplied output stream and exit
 *
 * @stream: output stream
 */
static void NORETURN usage(const char *progname, FILE *stream)
{
	assert(progname != NULL);
	assert(stream != NULL);

	fprintf(stream, "Usage: %s [-h] [-k KVM_PATH] [-m MEGABYTES] IMAGE\n",
		progname);

	exit(stream == stdout ? EXIT_SUCCESS : EXIT_FAILURE);
	/* NOTREACHED */
}

/**
 * parse_command_line() - parse command line arguments and return
 *                        configuration structure
 *
 * @argc: number of command line arguments
 * @argv: list of command line arguments
 *
 * Return: pointer to a parsed configuration structure
 */
static const struct config *parse_command_line(int argc, char *argv[])
{
	char *num_bytes_endptr;
	int opt;

	static struct config cfg = {
		.kvm_path   = DEFAULT_KVM_PATH,
		.image_path = DEFAULT_IMAGE_PATH,
		.num_bytes  = DEFAULT_NUM_BYTES
	};

	assert(argc > 0);
	assert(argv != NULL);

	while ((opt = getopt(argc, argv, "k:m:h")) != -1)
		switch (opt) {
		case 'k':
			cfg.kvm_path = optarg;
			break;
		case 'm':
			cfg.num_bytes = strtol(optarg, &num_bytes_endptr, 10);
			if (*num_bytes_endptr != '\0') {
				errorx("%s: wrong guest memory size", optarg);
				usage(argv[0], stderr);
				/* NOTREACHED */
			}
			cfg.num_bytes <<= 20;
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

	cfg.image_path = argv[optind];

	return &cfg;
}

/**
 * create_virtual_machine() - create a virtual machine
 *
 * @cfg:      parsed command line arguments
 * @kvm:      KVM subsystem descriptor
 * @guestmem: allocated guest memory of size cfg->num_bytes
 *
 * Return: virtual machine descriptor, or NULL if an error occurred
 */
static struct vm *create_virtual_machine(const struct config *cfg,
					 int kvm, void *guestmem)
{
	struct vm *vm;

	assert(cfg != NULL);
	assert(kvm > 0);
	assert(guestmem != NULL);

	vm = vm_create(kvm);
	if (vm == NULL)
		return NULL;

	if (vcpu_create(vm) < 0)
		goto err;

	if (vm_attach_memory(vm, 0x0, cfg->num_bytes, guestmem) < 0)
		goto err;

	if (binary_load(vm, cfg->image_path, 0,
			BINARY_LOAD_PROTECTED | BINARY_LOAD_PAGED) != 0)
		goto err;

	return vm;

err:
	vm_destroy(vm);
	return NULL;
}

/**
 * run_virtual_machine() - start a run loop for a virtual machine
 *
 * @vm: virtual machine to run
 *
 * Return: zero on clean virtual machine exit, or a non-zero value on error
 */
static int run_virtual_machine(struct vm *vm)
{
	struct kvm_run *vcpu;

	assert(vm != NULL);

	vcpu = vcpu_get(vm, BOOT_VCPU);
	for (/* NOTHING */; /* NOTHING */; /* NOTHING */) {
		if (vcpu_run(vm, 0) != 0)
			return EXIT_FAILURE;

		if (vcpu->exit_reason == KVM_EXIT_HLT)
			return EXIT_SUCCESS;

		if (vcpu->exit_reason == KVM_EXIT_IO &&
		    vcpu->io.port == 0x3f8 &&
		    vcpu->io.direction == KVM_EXIT_IO_OUT)
		{
			write(STDOUT_FILENO,
			      (const void *) vcpu + vcpu->io.data_offset,
			      vcpu->io.size * vcpu->io.count);
		}
	}

	/* NOTREACHED */
	return EXIT_FAILURE;
}

int main(int argc, char *argv[])
{
	const struct config *cfg;
	int ret = EXIT_FAILURE;
	void *guestmem;
	struct vm *vm;
	int kvm;

	cfg = parse_command_line(argc, argv);
	assert(cfg != NULL);

	kvm = kvm_open(cfg->kvm_path);
	if (kvm < 0)
		return EXIT_FAILURE;

	guestmem = mmap(0, cfg->num_bytes, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (guestmem == MAP_FAILED) {
		kvm_close(kvm);
		fail("mmap");
		/* NOTREACHED */
	}

	vm = create_virtual_machine(cfg, kvm, guestmem);
	if (vm != NULL) {
		ret = run_virtual_machine(vm);
		vm_destroy(vm);
	}

	munmap(guestmem, cfg->num_bytes);
	kvm_close(kvm);

	return ret;
}
