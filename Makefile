CC = gcc
AS = gcc
LD = gcc

CFLAGS  = -Wall -Werror -Wextra -Og -g -fsanitize=address -fno-omit-frame-pointer -I.
ASFLAGS = -m32
LDFLAGS = -Og -g -fsanitize=address -fno-omit-frame-pointer

OBJS = $(SRCS:.c=.o)
SRCS =                                                                       \
  kvm.c                                                                      \
  kvmapp.c                                                                   \
  loader/binary.c                                                            \
  log.c                                                                      \
  vcpu.c

GUESTS_OBJS = $(GUESTS:.S=.o)
GUESTS_BINS = $(GUESTS:.S=.bin)
GUESTS =                                                                     \
  guest/unrestricted_guest.S                                                 \
  guest/protected_guest.S

kvmapp: $(OBJS) $(GUESTS_BINS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

%.bin: %.o
	objcopy -O binary $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(AS) $(ASFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	@rm -f kvmapp $(GUESTS_OBJS) $(GUESTS_BINS) $(OBJS)
