.thumb
.thumb_func
.global _start
_start:
    // Clear bss
    ldr r0, =__bss_start__
    ldr r1, =__bss_end__
    mov r2, #0
__bss_loop_start:
    cmp r0, r1
    beq __copy_data
    str r2, [r0] // No post-index in Thumb-1
    add r0, r0, #4
    b __bss_loop_start

__copy_data:
    ldr r0, =__data_start__
    ldr r1, =__data_end__
    ldr r2, =__data_loadaddr_start__
__data_loop_start:
    cmp r0, r1
    beq __jump_main
    ldr r3, [r2] // No post-index
    str r3, [r0] // No post-index
    add r0, r0, #4
    add r2, r2, #4
    b __data_loop_start

__jump_main:
    bl main
