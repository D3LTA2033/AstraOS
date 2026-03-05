; User-space entry stub — ensures _start is at the beginning of the binary
section .text
global _start
extern user_main

_start:
    call user_main
    ; If user_main returns, call SYS_EXIT
    mov eax, 0          ; SYS_EXIT
    mov ebx, 0          ; exit code 0
    int 0x80
    jmp $               ; safety halt
