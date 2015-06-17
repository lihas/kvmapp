.code32
entry:
  cld
  movw  $0x3f8, %dx
  movl  $message, %esi
  movl  message_size, %ecx

print:
  lodsb
  outb  %al, %dx
  loop  print

halt:
  hlt
  jmp   halt

message:      .ascii "Hello protected KVMAPP!\n"
message_size: .word  . - message
