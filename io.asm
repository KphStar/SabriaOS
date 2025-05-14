section .text
global print_char
global print_string
global get_key

; ----------------------------------------
; void print_char(char c)
; Expects: char c in [esp + 32] (first argument after pusha)
print_char:
    pusha
    movzx eax, byte [esp + 32]  ; get char argument
    mov edi, [cursor_pos]
    add edi, 0xB8000            ; VGA base
    mov ah, 0x07                ; Light gray on black
    mov [edi], ax               ; Write character + attr
    add edi, 2
    sub edi, 0xB8000
    mov [cursor_pos], edi
    popa
    ret

; ----------------------------------------
; void print_string(const char* str)
; Expects: pointer to null-terminated string at [esp + 32]
print_string:
    pusha
    mov esi, [esp + 32]
    mov edi, [cursor_pos]
    add edi, 0xB8000

.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x07
    mov [edi], ax        ; correctly pairs char + attr
    add edi, 2
    jmp .loop

.done:
    sub edi, 0xB8000
    mov [cursor_pos], edi
    popa
    ret

; ----------------------------------------
; char get_key()
; Returns: ASCII in AL
get_key:
.wait_key:
    in al, 0x64
    test al, 1
    jz .wait_key
    in al, 0x60         ; Read from keyboard

    cmp al, 0x1C
    je .enter

    ; Lookup in scancode_table
    movzx eax, al
    mov al, [scancode_table + eax]
    ret

.enter:
    mov al, 0x0D        ; carriage return
    ret

; ----------------------------------------
section .bss
cursor_pos: resd 1

section .data
scancode_table:
    ; 0x00–0x0F
    db 0, 0, '1','2','3','4','5','6','7','8','9','0','-','=',0, 0
    ; 0x10–0x1F
    db 'Q','W','E','R','T','Y','U','I','O','P','[',']',0, 0, 'A','S'
    ; 0x20–0x2F
    db 'D','F','G','H','J','K','L',';', '\'', '`',0, '\\','Z','X','C','V'
    ; 0x30–0x3B
    db 'B','N','M',',','.','/',' ',0, 0, 0, 0, 0, 0, 0, 0, 0
    times 128 - ($ - scancode_table) db 0
