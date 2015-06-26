.code16
entry:
  cld
  movw  $0x3f8, %dx
  movw  $message, %si
  movw  message_size, %cx

print:
  lodsb
  outb  %al, %dx
  loop  print

halt:
  hlt
  jmp   halt

message:      .ascii "Hello unrestricted KVMAPP!\n"
message_size: .word  . - message
