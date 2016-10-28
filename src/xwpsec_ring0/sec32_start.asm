;
; sec32_start.asm:
;       this contains the head of the driver binary, most importantly,
;       the device driver header and the 16-bit strategy routine,
;       which calls the 32-bits sec32_strategy() routine.
;
;       This is linked FIRST by the makefile to make sure it appears
;       at the start of the binary.
;
; Copyright (C) 2000 Ulrich M”ller.
; Based on the MWDD32.SYS example sources,
; Copyright (C) 1995, 1996, 1997  Matthieu Willm (willm@ibm.net).
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, in version 2 as it comes in the COPYING
; file of the XWorkplace main distribution.
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.

        .386p


        INCL_DOSERRORS equ 1
        include bseerr.inc
        include devhlp.inc
        include devcmd.inc
        include devsym.inc

        include sec32_segdef.inc
        include r0thunk.inc

; Definition of the request packet header.

reqPacket       struc
reqLenght       db ?
reqUnit         db ?
reqCommand      db ?
reqStatus       dw ?
reqFlags        db ?
                db 3 dup (?)    ; Reserved field
reqLink         dd ?
reqPacket       ends

rpInitIn        struc
i_rph           db size reqPacket dup (?)
i_unit          db ?
i_devHelp       dd ?
i_initArgs      dd ?
i_driveNum      db ?
rpInitIn        ends

; *****************************************************
; *
; *     DATA16
; *
; *****************************************************

DATA16 segment
                extrn data16_end : byte
        public device_header

; *****************************************************
; *      Device Driver Header
; *****************************************************
device_header   dd -1                           ; Pointer to next driver
;               dw 1100100110000000b            ; Device attributes
                dw 1000100110000000b            ; Device attributes
;                  ||||| +-+   ||||
;                  ||||| | |   |||+------------------ is STDIN
;                  ||||| | |   ||+------------------- is STDOUT
;                  ||||| | |   |+-------------------- is NULL
;                  ||||| | |   +--------------------- is CLOCK
;                  ||||| | |
;                  ||||| | +------------------------+ (001) OS/2
;                  ||||| |                          | (010) DosDevIOCtl2 + SHUTDOWN
;                  ||||| +--------------------------+ (011) Capability bit strip
;                  |||||
;                  ||||+----------------------------- OPEN/CLOSE (char) or Removable (blk)
;                  |||+------------------------------ Sharing support
;                  ||+------------------------------- non-IBM block format (block only)
;                  |+-------------------------------- IDC capability
;                  +--------------------------------- device type bit: 0 = block, 1 = 1

                dw offset CODE16:sec32_stub_strategy; Strategy routine entry point
;                dw offset CODE16:sec32_stub_IDC     ; IDC routine entry point
                dw 0

                db 'XWPSEC$ '                   ; Device name
                db 8 dup (0)                    ; Reserved
                dw 0000000000011011b            ; Level 3 device drive capabilities
;                             |||||
;                             ||||+------------------ DosDevIOCtl2 + Shutdown
;                             |||+------------------- More than 16 MB support (char only)
;                             ||+-------------------- Parallel port driver
;                             |+--------------------- Adapter device driver
;                             +---------------------- InitComplete
                dw 0000000000000000b
DATA16 ends

; *****************************************************
; *
; *     CODE16
; *
; *****************************************************

CODE16 segment
        assume cs:CODE16, ds:DATA16

        extrn code16_end : byte

        public sec32_stub_strategy
;        public sec32_stub_IDC

;
; sec32_stub_strategy:
;       16-bit strategy routine, which calls
;       the 32-bit version.
;

sec32_stub_strategy proc far
        movzx eax, byte ptr es:[bx].reqCommand
        cmp eax, 0
        jz short @@error

        ; call int DRV32ENTRY sec32_strategy(PTR16 reqpkt, int index)
        push es                                 ; seg reqpkt
        push bx                                 ; ofs reqpkt
        push eax                                ; command
        mov word ptr es:[bx].reqStatus, 0       ; updates the request status
        call far ptr FLAT:SEC32_STRATEGY        ; 32 bits strategy entry point
                                                ; (sec32_strategy.c)
                                                ; _Far32 _Pascal _loadds,
                                                ; callee cleans up stack
        mov word ptr es:[bx].reqStatus, ax      ; updates the request status
        retf

@@error:
        ;
        ; Cannot be initialized as a DEVICE statement. MUST be
        ; initialized as a BASEDEV statement. (ring 0 FLAT CS is
        ; unreachable at DEVICE INIT time ...)
        ;
        mov dword ptr es:[bx].i_devHelp, 0
        mov  word ptr es:[bx].reqStatus, STDON + STERR + ERROR_I24_BAD_COMMAND
        retf
sec32_stub_strategy endp

;sec32_stub_IDC proc far
;        call far ptr FLAT:SEC32_IDC
;        retf
;sec32_stub_IDC endp

;*********************************************************************************************
;**************** Everything below this line will be unloaded after init *********************
;*********************************************************************************************
end_of_code:

CODE16 ends

CODE32 segment
ASSUME CS:FLAT, DS:FLAT, ES:FLAT

        public begin_code32

        extrn  SEC32_STRATEGY  : far
;        extrn  SEC32_IDC       : far

begin_code32:

CODE32 ends

DATA32 segment
        public  codeend
        public  dataend

        codeend dw offset CODE16:code16_end
        dataend dw offset DATA16:data16_end

DATA32 ends

BSS32 segment
        public  begin_data32

        begin_data32 dd (?)
BSS32 ends

end

