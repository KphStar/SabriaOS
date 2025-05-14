[bits 32]
[extern kmain]
[extern default_handler]
[extern timer_handler]
[extern keyboard_handler]
[extern double_fault_handler]
[extern syscall_handler]
[global _start]
[global default_handler_wrapper]
[global timer_handler_wrapper]
[global keyboard_handler_wrapper]
[global double_fault_handler_wrapper]
[global syscall_handler_wrapper]


section .text
_start:
    call kmain
    cli
    hlt

default_handler_wrapper:
    pusha
    call default_handler
    mov al, 0x20
    out 0x20, al
    popa
    iret

timer_handler_wrapper:
    pusha
    call timer_handler
    mov al, 0x20
    out 0x20, al
    popa
    iret

keyboard_handler_wrapper:
    pusha
    call keyboard_handler
    mov al, 0x20
    out 0x20, al
    popa
    iret

double_fault_handler_wrapper:
    pusha
    call double_fault_handler
    popa
    iret

syscall_handler_wrapper:
    pusha
    call syscall_handler
    popa
    iret

.clear_pde:
    mov [edi + esi], eax
    add esi, 4
    loop .clear_pde
    mov edi, 0x200000
    mov eax, 0x3
    mov ecx, 1024

.fill_pte:
    mov [edi], eax
    add eax, 0x1000
    add edi, 4
    loop .fill_pte
    mov eax, 0x100000
    mov cr3, eax
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    ret
    