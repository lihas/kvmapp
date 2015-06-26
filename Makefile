CC = gcc
AS = gcc
LD = gcc

CFLAGS  = -Wall -Werror -Wextra -O0 -g
ASFLAGS = -m32
LDFLAGS = -g

SRCS = kvmapp.c vm.c
OBJS = $(SRCS:.c=.o)

GUESTS_OBJS = $(GUESTS:.S=.o)
GUESTS_BINS = $(GUESTS:.S=.bin)
GUESTS_HS   = $(GUESTS:.S=.bin.h)
GUESTS =                                                                     \
  guest/unrestricted_guest.S                                                 \
  guest/protected_guest.S

kvmapp: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

kvmapp.o: $(GUESTS_HS)

%.bin.h: %.bin
	xxd -i $< $@

%.bin: %.o
	objcopy -O binary $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(AS) $(ASFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	@rm -f kvmapp $(GUESTS_OBJS) $(GUESTS_BINS) $(GUESTS_HS) $(OBJS)
