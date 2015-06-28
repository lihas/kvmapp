#ifndef _KVM_H
#define _KVM_H

#include <stddef.h>
#include <stdint.h>

struct vm;
struct kvm_run;
struct kvm_regs;
struct kvm_sregs;

int kvm_open(const char *);
void kvm_close(int);

struct vm *vm_create(int);
int vm_attach_memory(struct vm *, uintptr_t, size_t, void *);
void *vm_get_memory(struct vm *, uintptr_t, size_t);
void vm_destroy(struct vm *);

int vcpu_create(struct vm *);
int vcpu_get_regs(struct vm *, unsigned, struct kvm_regs *);
int vcpu_set_regs(struct vm *, unsigned, const struct kvm_regs *);
int vcpu_get_sregs(struct vm *, unsigned, struct kvm_sregs *);
int vcpu_set_sregs(struct vm *, unsigned, const struct kvm_sregs *);
struct kvm_run *vcpu_get(struct vm *, unsigned);
int vcpu_run(struct vm *, unsigned);

#endif /* _KVM_H */
