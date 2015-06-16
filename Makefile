CC = gcc
AS = gcc
LD = gcc

CFLAGS  = -Wall -Werror -O0 -g
ASFLAGS = -m32
LDFLAGS = -g

SRCS = kvmapp.c
OBJS = $(SRCS:.c=.o)

kvmapp: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

kvmapp.o: unrestricted_guest.bin.h

unrestricted_guest.bin.h: unrestricted_guest.bin
	xxd -i $< $@

unrestricted_guest.bin: unrestricted_guest.o
	objcopy -O binary $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(AS) $(ASFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	@rm -f kvmapp unrestricted_guest.o unrestricted_guest.bin unrestricted_guest.bin.h $(OBJS)
