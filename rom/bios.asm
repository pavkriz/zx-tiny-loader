#target rom

COMMAND_NONE equ 0          ; no command
COMMAND_MEM_COPY equ 1      ; copy from buffer to memory
COMMAND_JUMP equ 2          ; jump to buffer address
COMMAND_SNAP equ 3          ; load snapshot (copy data to RAM via BYTE_BUFFER and jump to ROM_BUFFER_ADDR)

ROM_BUFFER_ADDR equ 0x2000
ROM_BUFFER_SIZE equ 0x2000

BORDER_COLOR_WHITE equ 0x7
BORDER_COLOR_BLACK equ 0x0
BORDER_COLOR_BLUE equ 0x1
BORDER_COLOR_RED equ 0x2
BORDER_COLOR_MAGENTA equ 0x3
BORDER_COLOR_GREEN equ 0x4
BORDER_COLOR_CYAN equ 0x5
BORDER_COLOR_YELLOW equ 0x6

PORT_ULA equ 0xFE

#data VARIABLES, 0x1F00

; define some variables here
COMMAND:
        defb 0
BYTE_BUFFER:
        defb 0
COMMAND_DEST:   ; usually destination address where to copy the current buffer
        defw 0
COMMAND_SIZE:   ; usually size of the current buffer
        defw 0


#code EPROM, 0, 0x4000


; reset vector
RST0::  di
        jp      init
        defs    0x08-$

RST1::  ret
        defs    0x10-$

RST2::  ret
        defs    0x18-$

RST3::  ret
        defs    0x20-$

RST4::  ret
        defs    0x28-$

RST5::  ret
        defs    0x30-$
	ret

RST6::  ret
        defs    0x38-$

; maskable interrupt handler
RST7::  ei
        reti

; non maskable interrupt:
; e.g. call debugger and on exit resume.
        defs    0x66-$
NMI::   ei                      ; else re-eanble interrupts
        ret


; init:
init:
        ; initialize the stack pointer
        ld sp, 0xFFFF
        ld a, BORDER_COLOR_WHITE
        out (PORT_ULA), a
command_loop:
        ld a, BORDER_COLOR_YELLOW
        out (PORT_ULA), a

        ld a, (COMMAND)
        cp COMMAND_NONE
        jr z, command_loop
        cp COMMAND_MEM_COPY
        jr z, command_mem_copy
        cp COMMAND_JUMP
        jr z, command_jump
        cp COMMAND_SNAP
        jr z, command_snap_load
        ; unknown command
        jp command_loop
command_mem_copy:
        ; copy the buffer to the destination
        ld hl, ROM_BUFFER_ADDR
        ld de, (COMMAND_DEST)
        ld bc, (COMMAND_SIZE)
        ldir
        ; reset command
        ld a, COMMAND_NONE
        ld (COMMAND), a
        jp command_loop
command_jump:
        ; jump to buffer where the code to continue in execution is
        jp ROM_BUFFER_ADDR
        ; this is usually the last command that eg. bootstraps the loaded snapshot
command_snap_load:
        ld a, BORDER_COLOR_BLUE
        out (PORT_ULA), a
        ; snap loading phase1: load the snapshot memory to RAM via BYTE_BUFFER
        ld hl, BYTE_BUFFER+1  ; source address (+1 because we decremented HL in the loop before LDI)
        ld de, 0x4000  ; fixed destination address (RAM)
        ld bc, 0xC000  ; fixed size of the snapshot (48k)
_command_snap_load_loop:
        dec hl         ; decrement the source address back to BYTE_BUFFER in th eloop (since each LDI increments HL automatically)
        ldi            ; load one byte from BYTE_BUFFER to RAM 
        jp pe, _command_snap_load_loop ; loop until all bytes are copied
        ; snap loading phase2: jump to buffer where the code to continue in execution is (sets registers, etc.)
        ld a, BORDER_COLOR_GREEN
        out (PORT_ULA), a
        jp ROM_BUFFER_ADDR
        ; this is usually the last command that eg. bootstraps the loaded snapshot 