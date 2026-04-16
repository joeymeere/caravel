.globl __divsi3
.type __divsi3, @function
.globl __divdi3
.type __divdi3, @function
.globl __modsi3
.type __modsi3, @function
.globl __moddi3
.type __moddi3, @function

__divsi3:
    lsh64 r1, 32
    arsh64 r1, 32
    lsh64 r2, 32
    arsh64 r2, 32

__divdi3:
    mov64 r3, r1
    arsh64 r3, 63
    mov64 r4, r2
    arsh64 r4, 63
    mov64 r5, r3
    xor64 r5, r4

    xor64 r1, r3
    sub64 r1, r3
    xor64 r2, r4
    sub64 r2, r4

    mov64 r0, r1
    div64 r0, r2

    xor64 r0, r5
    sub64 r0, r5
    exit

__modsi3:
    lsh64 r1, 32
    arsh64 r1, 32
    lsh64 r2, 32
    arsh64 r2, 32

__moddi3:
    mov64 r3, r1
    arsh64 r3, 63
    mov64 r4, r2
    arsh64 r4, 63

    xor64 r1, r3
    sub64 r1, r3
    xor64 r2, r4
    sub64 r2, r4

    mov64 r0, r1
    mod64 r0, r2

    xor64 r0, r3
    sub64 r0, r3
    exit
