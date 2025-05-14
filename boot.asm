[bits 16]
[org 0x7C00]

; Bootloader entry point
start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Save boot drive number (passed by BIOS in dl)
    mov [boot_drive], dl

    ; Print loading message
    mov si, loading_msg
    call print_string

    ; Print drive number for debugging
    mov si, drive_msg
    call print_string
    mov al, [boot_drive]
    call print_hex
    mov si, newline
    call print_string

    ; Load kernel from CD using extended read (int 0x13, AH=0x42)
    mov si, dap             ; Address of Disk Address Packet
    mov ah, 0x42            ; Extended read function
    mov dl, [boot_drive]    ; Use the boot drive number
    int 0x13
    jc disk_error

    ; Print success message
    mov si, success_msg
    call print_string

    ; Switch to protected mode
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:protected_mode

[bits 32]
protected_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x9000

    ; Jump to kernel at 0x1000
    jmp 0x1000

    cli
    hlt

; Disk read error
disk_error:
    mov si, error_msg
    call print_string
    ; Print error code (in AH)
    mov al, ah
    call print_hex
    mov si, newline
    call print_string
    ; Print drive number
    mov si, drive_msg
    call print_string
    mov al, [boot_drive]
    call print_hex
    mov si, newline
    call print_string
    jmp $

; Simple print function (16-bit real mode)
print_string:
    lodsb
    cmp al, 0
    je done_print
    mov ah, 0x0E
    int 0x10
    jmp print_string
done_print:
    ret

; Print a byte in hex (for debugging)
print_hex:
    mov ah, 0x0E
    mov bl, al
    shr al, 4
    call print_nibble
    mov al, bl
    and al, 0x0F
    call print_nibble
    ret

print_nibble:
    cmp al, 10
    jl digit
    add al, 'A' - 10
    jmp print_char
digit:
    add al, '0'
print_char:
    mov ah, 0x0E
    int 0x10
    ret

loading_msg: db "Loading kernel...", 0x0D, 0x0A, 0
drive_msg: db "Drive: ", 0
error_msg: db "Disk error: ", 0
success_msg: db "Kernel loaded!", 0x0D, 0x0A, 0
newline: db 0x0D, 0x0A, 0

; Disk Address Packet (DAP) for int 0x13, AH=0x42
dap:
    db 0x10         ; Size of DAP (16 bytes)
    db 0            ; Unused
    dw 10           ; Number of sectors to read (9584 bytes = 19 sectors)
    dd 0x1000       ; Destination address (0x1000)
    dq 35           ; LBA 35 (confirmed correct)

; Boot drive number storage
boot_drive: db 0

; GDT (Global Descriptor Table)
gdt_start:
    dq 0x0 ; Null descriptor
gdt_code:
    dw 0xFFFF
    dw 0x0
    db 0x0
    db 10011010b
    db 11001111b
    db 0x0
gdt_data:
    dw 0xFFFF
    dw 0x0
    db 0x0
    db 10010010b
    db 11001111b
    db 0x0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; Boot sector signature
times 510-($-$$) db 0
dw 0xAA55