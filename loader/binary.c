#include <assert.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/user.h>
#include <unistd.h>

#include <linux/kvm.h>

#include "binary.h"
#include "kvm.h"
#include "kvmapp.h"
#include "vcpu.h"

/**
 * load_image() - load binary file into a virtual machine
 *
 * @vm:   virtual machine descriptor
 * @path: path to a binary file
 * @base: guest physical load address
 *
 * Return: size of loaded image, or -1 if an error occurred
 */
static ssize_t load_image(struct vm *vm, const char *path, uintptr_t base)
{
	ssize_t ret = -1;
	struct stat st;
	void *dst;
	int fd;

	fd = open(path, O_RDWR);
	if (fd > 0) {
		if (fstat(fd, &st) == 0) {
			dst = vm_get_memory(vm, base, st.st_size);
			if (dst != NULL)
				ret = read(fd, dst, st.st_size);
		}

		close(fd);
	}

	return ret;
}

/**
 * binary_load() - bootstrap virtual machine from a binary file
 *
 * @vm:    virtual machine descriptor
 * @path:  path to a binary file with bootstrap code
 * @base:  guest physical load address
 * @flags: loader flags, specifying initial machine state
 *
 * Return: zero on success, or -1 if an error occurred
 */
int binary_load(struct vm *vm, const char *path, uintptr_t base, int flags)
{
	ssize_t image_size;
	uintptr_t pdir;

	assert(vm != NULL);
	assert(path != NULL);
	assert((flags & ~(BINARY_LOAD_PROTECTED | BINARY_LOAD_PAGED)) == 0);

	image_size = load_image(vm, path, base);
	if (image_size < 0)
		return -1;

	if (vcpu_init(vm, BOOTSTRAP_VCPU, base) != 0)
		return -1;

	if ((flags & BINARY_LOAD_PROTECTED) != 0)
		if (vcpu_enable_protected_mode(vm, BOOTSTRAP_VCPU) != 0)
			return -1;

	if ((flags & BINARY_LOAD_PAGED) != 0) {
		pdir = round_up(base + image_size, PAGE_SIZE);
		if (vcpu_enable_paged_mode(vm, BOOTSTRAP_VCPU, pdir) != 0)
			return -1;
	}

	return 0;
}
