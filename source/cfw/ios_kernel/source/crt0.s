.section ".init"
.arm
.align 4

.extern mainKernel
.type mainKernel, %function

_start:
    mov r0, #1
    b mainKernel
