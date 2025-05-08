; ================================================================
;   Example source with target 'rom'
;   Copyright  (c)  Günter Woigk 1994 - 2017
;                   mailto:kio@little-bat.de
; ================================================================


; same as 'bin', except that the default fill byte for 'defs' etc. is 0xff
; this example defines a 16k Eprom visible at address 0x0000 and an area
; for variables at 0x5B00 upward, which may be used for the ZX Spectrum


#target rom


#data VARIABLES, 0x5B00

; define some variables here
VAR1:
        defb 0
VAR2:
        defb 0
results:
    DS 1000    ; místo pro výsledky


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

; maskable interrupt handler in interrupt mode 1:
RST7::  ld      a, r
        ld      (VAR2), a            ; store R register to RAM (check the shadow-emulated RAM WR here)
        ;jr      $                    ; infinite loop
        ei
        reti


; non maskable interrupt:
; e.g. call debugger and on exit resume.

        defs    0x66-$
NMI::   ld      a,i
        push    af
        pop     af
        ret     po              ; interrupts were disabled
        ei                      ; else re-eanble interrupts
        ret


; init:
; globals and statics initialization
; starts with copying the fixed data:

init:
        ld      a,r
        ld      (VAR1), a            ; store R register to RAM (check the shadow-emulated RAM WR here)
        ld      sp,$ffff

z80_undoc:
; Start - inicializace
    LD HL, results   ; Kam budeme ukládat výsledky

; --- Test 1: Undocumented LD instructions ---
    LD A, 0x55
    LD E, A
    LD D, 0xAA

    LD (HL), A       ; normální zápis
    INC HL

    ; Použijeme nedokumentovanou instrukci LD (IX+0),A
    ; Prefix DD + 77 00
    DB 0xDD, 0x77, 0x00
    ; Pokud CPU ignoruje 0xDD správně -> žádný problém
    ; Špatné klony tu můžou spadnout nebo špatně zapsat

    LD (HL), A       ; zapíšeme hodnotu po "trap testu"
    INC HL

; --- Test 2: DD/FD prefix flooding ---
    LD BC, 0x1234
;    ; Hodně DD prefixů před normální instrukcí
;    DB 0xDD, 0xDD, 0xDD, 0x01, 0x78, 0x56  ; LD BC,5678h ?
; my Z80 clone do not handle this correctly

    LD (HL), B       ; uložíme B
    INC HL
    LD (HL), C       ; uložíme C
    INC HL

; --- Test 3: R register behavior ---
    LD A, R          ; Přečteme registr R
    LD (HL), A
    INC HL

    ; proveď nějakou instrukci, která by měla zvýšit R
    NOP
    NOP
    NOP

    LD A, R
    LD (HL), A
    INC HL

flags_bit_3_5_test:
    LD A, 0x00
    SCF
    PUSH AF
;    INC HL

    LD A, 0xFF
    SCF
    PUSH AF
;    INC HL

    LD A, 0xAA
    CPL
    PUSH AF
;    INC HL

    LD A, 0x55
    CPL
    PUSH AF
;    INC HL

io_timing_test:
        ; --- Test 4: T-states measurement using R ---

; Připrav port a registr
    LD BC, 0x00FE

; --- Měření NOP 8x ---
    LD A, R            ; Načti počáteční R
    LD D, A

    LD B, 8
Measure_NOP_R:
    NOP
    DJNZ Measure_NOP_R

    LD A, R            ; Načti koncové R
    SUB D
    LD (HL), A         ; Výsledek rozdílu po NOP do results
    INC HL

; --- Měření IN A,(C) 8x ---
    LD BC, 0x00FE
    LD A, R            ; Načti počáteční R
    LD D, A

    LD B, 8
Measure_IN_R:
    IN A,(C)
    DJNZ Measure_IN_R

    LD A, R
    SUB D
    LD (HL), A         ; Výsledek rozdílu po IN do results
    INC HL

; --- Měření OUT (C),A 8x ---
    LD BC, 0x00FE
    LD A, 0xA5         ; Hodnota k zápisu

    LD A, R            ; Načti počáteční R
    LD D, A

    LD B, 8
Measure_OUT_R:
    OUT (C),A
    DJNZ Measure_OUT_R

    LD A, R
    SUB D
    LD (HL), A         ; Výsledek rozdílu po OUT do results
    INC HL

irq_test:

        ;im      1                    ; set IM 1 mode
        call setup_im2
        im      2                    ; set IM 2 mode
        ei                           ; enable maskable IRQ
        jr      $                    ; and wait (for interrupt)


; Make sure this is on a 256 byte boundary
        ORG           $2000
IM2Table:
        defs          257,$30  ; high byte of the IM2Routine address

; Make sure this is on 0xMNMN boundary sice ZX Spectrum cannot guarantee even number on DATA bus during IRQ ACK so we may not get word-aligned address
        ORG           $3030

; Basically nothing
IM2Routine:  
        ld      a, r
        ld      (VAR2), a            ; store R register to RAM (check the shadow-emulated RAM WR here)
        ;jr      $                    ; infinite loop
        ei
        reti



setup_im2:

; Setup the 128 entry vector table
              di

              ld            hl, IM2Table
              ld            de, IM2Table+1
              ld            bc, 256

              ; Setup the I register (the high byte of the table)
              ld            a, h
              ld            i, a

              ; filling the table with the address of the IM2Routine makes no sense for in-ROM version, see fixed $30 value in IM2Table

              ; Set the first entries in the table to $FC
              ;ld            a, $FC
              ;ld            (hl), a

              ; Copy to all the remaining 256 bytes
              ;ldir


              ret



