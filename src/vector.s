// https://developer.arm.com/documentation/dui0662/b/The-Cortex-M0--Processor/Exception-model/Vector-table
// RM0490 Table 55 for ARM + ST
.section .vector, "a"
.word 0x20006000 // sp = end of SRAM
.word _start
.rept 22
.word _unsupported_handler
.endr
.word _usb_handler
.rept 23
.word _unsupported_handler
.endr

.section .text
.thumb
.thumb_func
_unsupported_handler:
    b _unsupported_handler

.thumb_func
_usb_handler:
    push {lr}
    bl board_usb_handler
    pop {pc}
