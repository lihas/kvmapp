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

kvmapp.o: guest.bin.h

guest.bin.h: guest.bin
	xxd -i $< $@

guest.bin: guest.o
	objcopy -O binary $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(AS) $(ASFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	@rm -f kvmapp guest.o guest.bin guest.bin.h $(OBJS)
