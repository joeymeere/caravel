.globl mul_div_u64
.type mul_div_u64, @function

.globl div_wide
.type div_wide, @function

mul_div_u64:
    stxdw [r10 - 8], r6
    stxdw [r10 - 16], r7
    stxdw [r10 - 24], r8
    stxdw [r10 - 32], r9
    stxdw [r10 - 40], r3

    mov64 r4, r1
    lsh64 r4, 32
    rsh64 r4, 32
    rsh64 r1, 32

    mov64 r5, r2
    lsh64 r5, 32
    rsh64 r5, 32
    rsh64 r2, 32

    mov64 r6, r4
    mul64 r6, r5

    mov64 r7, r1
    mul64 r7, r5

    mul64 r4, r2
    mul64 r1, r2

    mov64 r3, r6
    rsh64 r3, 32
    mov64 r5, r7
    lsh64 r5, 32
    rsh64 r5, 32
    add64 r3, r5
    mov64 r5, r4
    lsh64 r5, 32
    rsh64 r5, 32
    add64 r3, r5

    lsh64 r6, 32
    rsh64 r6, 32
    mov64 r5, r3
    lsh64 r5, 32
    or64 r6, r5

    rsh64 r7, 32
    add64 r1, r7
    rsh64 r4, 32
    add64 r1, r4
    rsh64 r3, 32
    add64 r1, r3

    ldxdw r8, [r10 - 40]

    jne r1, 0, .Lmuldiv_slow

    mov64 r0, r6
    div64 r0, r8
    ldxdw r6, [r10 - 8]
    ldxdw r7, [r10 - 16]
    ldxdw r8, [r10 - 24]
    ldxdw r9, [r10 - 32]
    exit

.Lmuldiv_slow:
    mov64 r2, r6
    mov64 r3, r8
    ldxdw r6, [r10 - 8]
    ldxdw r7, [r10 - 16]
    ldxdw r8, [r10 - 24]
    ldxdw r9, [r10 - 32]

div_wide:
    stxdw [r10 - 8], r6
    stxdw [r10 - 16], r7
    stxdw [r10 - 24], r8
    stxdw [r10 - 32], r9

    jne r1, 0, .Ldw_slow
    mov64 r0, r2
    div64 r0, r3
    ja .Ldw_ret

.Ldw_slow:
    mov64 r0, 0
    mov64 r8, r3
    mov64 r4, 1
    lsh64 r4, 63

.Lbit_loop:
    lsh64 r1, 1
    mov64 r3, r2
    rsh64 r3, 63
    or64 r1, r3
    lsh64 r2, 1
    jlt r1, r8, .Lbit_skip
    sub64 r1, r8
    or64 r0, r4
.Lbit_skip:
    rsh64 r4, 1
    jne r4, 0, .Lbit_loop

.Ldw_ret:
    ldxdw r6, [r10 - 8]
    ldxdw r7, [r10 - 16]
    ldxdw r8, [r10 - 24]
    ldxdw r9, [r10 - 32]
    exit
