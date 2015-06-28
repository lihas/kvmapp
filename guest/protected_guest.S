#define UART_PORT 0x3f8

.code32

entry:
  pushl message_size
  pushl $message
  call  put_string
  subl  $8, %esp

  call  halt

put_string:
  cld
  movw  $UART_PORT, %dx
  movl  4(%esp), %esi
  movl  8(%esp), %ecx

1:
  lodsb
  outb  %al, %dx
  loop  1b

  retl

halt:
  hlt
  jmp   halt

message:      .ascii "Hello protected KVMAPP!\n"
message_size: .word  . - message
