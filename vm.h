#ifndef _VM_H
#define _VM_H

#include <stddef.h>
#include <stdint.h>

struct vm;
struct kvm_run;
struct kvm_regs;
struct kvm_sregs;

struct vm *vm_create(int);
void vm_destroy(struct vm *);

int vm_attach_memory(struct vm *, uintptr_t, size_t, void *);
void *vm_get_memory(struct vm *, uintptr_t, size_t);

int vm_create_vcpu(struct vm *);
int vm_get_regs(struct vm *, unsigned, struct kvm_regs *);
int vm_set_regs(struct vm *, unsigned, const struct kvm_regs *);
int vm_get_sregs(struct vm *, unsigned, struct kvm_sregs *);
int vm_set_sregs(struct vm *, unsigned, const struct kvm_sregs *);
struct kvm_run *vm_get_vcpu(struct vm *, unsigned);
int vm_run(struct vm *, unsigned);

#endif /* _VM_H */
