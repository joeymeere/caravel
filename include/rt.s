.globl __divsi3
.type __divsi3, @function
.globl __divdi3
.type __divdi3, @function
.globl __modsi3
.type __modsi3, @function
.globl __moddi3
.type __moddi3, @function
.globl __udivsi3
.type __udivsi3, @function
.globl __udivdi3
.type __udivdi3, @function
.globl __umodsi3
.type __umodsi3, @function
.globl __umoddi3
.type __umoddi3, @function
.globl __multi3
.type __multi3, @function
.globl __ashlti3
.type __ashlti3, @function
.globl __lshrti3
.type __lshrti3, @function
.globl __ashrti3
.type __ashrti3, @function
.globl __udivti3
.type __udivti3, @function
.globl __umodti3
.type __umodti3, @function
.globl __divti3
.type __divti3, @function
.globl __modti3
.type __modti3, @function

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

__udivsi3:
    lsh64 r1, 32
    rsh64 r1, 32
    lsh64 r2, 32
    rsh64 r2, 32

__udivdi3:
    mov64 r0, r1
    div64 r0, r2
    exit

__umodsi3:
    lsh64 r1, 32
    rsh64 r1, 32
    lsh64 r2, 32
    rsh64 r2, 32

__umoddi3:
    mov64 r0, r1
    mod64 r0, r2
    exit

__ashlti3:
    jge r4, 64, .Lashl_big
    jeq r4, 0, .Lashl_zero
    mov64 r5, r2
    lsh64 r3, r4
    mov64 r0, 64
    sub64 r0, r4
    rsh64 r5, r0
    or64 r3, r5
    lsh64 r2, r4
    stxdw [r1+0], r2
    stxdw [r1+8], r3
    exit
.Lashl_big:
    sub64 r4, 64
    lsh64 r2, r4
    stxdw [r1+8], r2
    mov64 r2, 0
    stxdw [r1+0], r2
    exit
.Lashl_zero:
    stxdw [r1+0], r2
    stxdw [r1+8], r3
    exit

__lshrti3:
    jge r4, 64, .Llshr_big
    jeq r4, 0, .Llshr_zero
    mov64 r5, r3
    rsh64 r2, r4
    mov64 r0, 64
    sub64 r0, r4
    lsh64 r5, r0
    or64 r2, r5
    rsh64 r3, r4
    stxdw [r1+0], r2
    stxdw [r1+8], r3
    exit
.Llshr_big:
    sub64 r4, 64
    rsh64 r3, r4
    stxdw [r1+0], r3
    mov64 r3, 0
    stxdw [r1+8], r3
    exit
.Llshr_zero:
    stxdw [r1+0], r2
    stxdw [r1+8], r3
    exit

__ashrti3:
    jge r4, 64, .Lashr_big
    jeq r4, 0, .Lashr_zero
    mov64 r5, r3
    rsh64 r2, r4
    mov64 r0, 64
    sub64 r0, r4
    lsh64 r5, r0
    or64 r2, r5
    arsh64 r3, r4
    stxdw [r1+0], r2
    stxdw [r1+8], r3
    exit
.Lashr_big:
    sub64 r4, 64
    mov64 r2, r3
    arsh64 r2, r4
    arsh64 r3, 63
    stxdw [r1+0], r2
    stxdw [r1+8], r3
    exit
.Lashr_zero:
    stxdw [r1+0], r2
    stxdw [r1+8], r3
    exit

__multi3:
    stxdw [r10-8], r6
    stxdw [r10-16], r7
    stxdw [r10-24], r8
    stxdw [r10-32], r9
    mov64 r6, r1

    mov64 r0, r2
    mul64 r0, r4
    stxdw [r6+0], r0

    mov64 r1, r3
    mul64 r1, r4
    mul64 r5, r2
    add64 r1, r5

    mov64 r7, r2
    lsh64 r7, 32
    rsh64 r7, 32
    rsh64 r2, 32
    mov64 r8, r4
    lsh64 r8, 32
    rsh64 r8, 32
    rsh64 r4, 32

    mov64 r9, r7
    mul64 r9, r8
    rsh64 r9, 32

    mov64 r3, r2
    mul64 r3, r8
    add64 r3, r9

    mov64 r5, r3
    rsh64 r5, 32
    lsh64 r3, 32
    rsh64 r3, 32
    mul64 r7, r4
    add64 r3, r7
    rsh64 r3, 32

    mul64 r2, r4
    add64 r2, r5
    add64 r2, r3
    add64 r2, r1
    stxdw [r6+8], r2

    ldxdw r6, [r10-8]
    ldxdw r7, [r10-16]
    ldxdw r8, [r10-24]
    ldxdw r9, [r10-32]
    exit

__udivti3:
    stxdw [r10-8], r6
    stxdw [r10-16], r7
    stxdw [r10-24], r8
    stxdw [r10-32], r9
    stxdw [r10-40], r1
    stxdw [r10-48], r4
    stxdw [r10-56], r5
    mov64 r8, 0
    mov64 r9, 0
    mov64 r6, 128
.Ludiv_loop:
    lsh64 r9, 1
    mov64 r0, r8
    rsh64 r0, 63
    or64 r9, r0
    lsh64 r8, 1
    mov64 r0, r3
    rsh64 r0, 63
    or64 r8, r0
    lsh64 r3, 1
    mov64 r0, r2
    rsh64 r0, 63
    or64 r3, r0
    lsh64 r2, 1
    ldxdw r4, [r10-48]
    ldxdw r5, [r10-56]
    jgt r9, r5, .Ludiv_sub
    jne r9, r5, .Ludiv_skip
    jlt r8, r4, .Ludiv_skip
.Ludiv_sub:
    mov64 r0, r8
    sub64 r8, r4
    sub64 r9, r5
    jge r0, r4, .Ludiv_nb
    sub64 r9, 1
.Ludiv_nb:
    or64 r2, 1
.Ludiv_skip:
    add64 r6, -1
    jne r6, 0, .Ludiv_loop
    ldxdw r1, [r10-40]
    stxdw [r1+0], r2
    stxdw [r1+8], r3
    ldxdw r6, [r10-8]
    ldxdw r7, [r10-16]
    ldxdw r8, [r10-24]
    ldxdw r9, [r10-32]
    exit

__umodti3:
    stxdw [r10-8], r6
    stxdw [r10-16], r7
    stxdw [r10-24], r8
    stxdw [r10-32], r9
    stxdw [r10-40], r1
    stxdw [r10-48], r4
    stxdw [r10-56], r5
    mov64 r8, 0
    mov64 r9, 0
    mov64 r6, 128
.Lumod_loop:
    lsh64 r9, 1
    mov64 r0, r8
    rsh64 r0, 63
    or64 r9, r0
    lsh64 r8, 1
    mov64 r0, r3
    rsh64 r0, 63
    or64 r8, r0
    lsh64 r3, 1
    mov64 r0, r2
    rsh64 r0, 63
    or64 r3, r0
    lsh64 r2, 1
    ldxdw r4, [r10-48]
    ldxdw r5, [r10-56]
    jgt r9, r5, .Lumod_sub
    jne r9, r5, .Lumod_skip
    jlt r8, r4, .Lumod_skip
.Lumod_sub:
    mov64 r0, r8
    sub64 r8, r4
    sub64 r9, r5
    jge r0, r4, .Lumod_nb
    sub64 r9, 1
.Lumod_nb:
    or64 r2, 1
.Lumod_skip:
    add64 r6, -1
    jne r6, 0, .Lumod_loop
    ldxdw r1, [r10-40]
    stxdw [r1+0], r8
    stxdw [r1+8], r9
    ldxdw r6, [r10-8]
    ldxdw r7, [r10-16]
    ldxdw r8, [r10-24]
    ldxdw r9, [r10-32]
    exit

__divti3:
    stxdw [r10-8], r6
    stxdw [r10-16], r7
    stxdw [r10-24], r8
    stxdw [r10-32], r9
    stxdw [r10-40], r1
    mov64 r7, r3
    xor64 r7, r5
    arsh64 r7, 63
    mov64 r0, r3
    arsh64 r0, 63
    xor64 r2, r0
    xor64 r3, r0
    sub64 r2, r0
    mov64 r1, 0
    jeq r0, 0, .Lsdiv_aa
    jne r2, 0, .Lsdiv_aa
    mov64 r1, 1
.Lsdiv_aa:
    add64 r3, r1
    mov64 r0, r5
    arsh64 r0, 63
    xor64 r4, r0
    xor64 r5, r0
    sub64 r4, r0
    mov64 r1, 0
    jeq r0, 0, .Lsdiv_ab
    jne r4, 0, .Lsdiv_ab
    mov64 r1, 1
.Lsdiv_ab:
    add64 r5, r1
    stxdw [r10-48], r4
    stxdw [r10-56], r5
    mov64 r8, 0
    mov64 r9, 0
    mov64 r6, 128
.Lsdiv_loop:
    lsh64 r9, 1
    mov64 r0, r8
    rsh64 r0, 63
    or64 r9, r0
    lsh64 r8, 1
    mov64 r0, r3
    rsh64 r0, 63
    or64 r8, r0
    lsh64 r3, 1
    mov64 r0, r2
    rsh64 r0, 63
    or64 r3, r0
    lsh64 r2, 1
    ldxdw r4, [r10-48]
    ldxdw r5, [r10-56]
    jgt r9, r5, .Lsdiv_sub
    jne r9, r5, .Lsdiv_skip
    jlt r8, r4, .Lsdiv_skip
.Lsdiv_sub:
    mov64 r0, r8
    sub64 r8, r4
    sub64 r9, r5
    jge r0, r4, .Lsdiv_nb
    sub64 r9, 1
.Lsdiv_nb:
    or64 r2, 1
.Lsdiv_skip:
    add64 r6, -1
    jne r6, 0, .Lsdiv_loop
    xor64 r2, r7
    xor64 r3, r7
    sub64 r2, r7
    mov64 r0, 0
    jeq r7, 0, .Lsdiv_done
    jne r2, 0, .Lsdiv_done
    mov64 r0, 1
.Lsdiv_done:
    add64 r3, r0
    ldxdw r1, [r10-40]
    stxdw [r1+0], r2
    stxdw [r1+8], r3
    ldxdw r6, [r10-8]
    ldxdw r7, [r10-16]
    ldxdw r8, [r10-24]
    ldxdw r9, [r10-32]
    exit

__modti3:
    stxdw [r10-8], r6
    stxdw [r10-16], r7
    stxdw [r10-24], r8
    stxdw [r10-32], r9
    stxdw [r10-40], r1
    mov64 r7, r3
    arsh64 r7, 63
    mov64 r0, r3
    arsh64 r0, 63
    xor64 r2, r0
    xor64 r3, r0
    sub64 r2, r0
    mov64 r1, 0
    jeq r0, 0, .Lsmod_aa
    jne r2, 0, .Lsmod_aa
    mov64 r1, 1
.Lsmod_aa:
    add64 r3, r1
    mov64 r0, r5
    arsh64 r0, 63
    xor64 r4, r0
    xor64 r5, r0
    sub64 r4, r0
    mov64 r1, 0
    jeq r0, 0, .Lsmod_ab
    jne r4, 0, .Lsmod_ab
    mov64 r1, 1
.Lsmod_ab:
    add64 r5, r1
    stxdw [r10-48], r4
    stxdw [r10-56], r5
    mov64 r8, 0
    mov64 r9, 0
    mov64 r6, 128
.Lsmod_loop:
    lsh64 r9, 1
    mov64 r0, r8
    rsh64 r0, 63
    or64 r9, r0
    lsh64 r8, 1
    mov64 r0, r3
    rsh64 r0, 63
    or64 r8, r0
    lsh64 r3, 1
    mov64 r0, r2
    rsh64 r0, 63
    or64 r3, r0
    lsh64 r2, 1
    ldxdw r4, [r10-48]
    ldxdw r5, [r10-56]
    jgt r9, r5, .Lsmod_sub
    jne r9, r5, .Lsmod_skip
    jlt r8, r4, .Lsmod_skip
.Lsmod_sub:
    mov64 r0, r8
    sub64 r8, r4
    sub64 r9, r5
    jge r0, r4, .Lsmod_nb
    sub64 r9, 1
.Lsmod_nb:
    or64 r2, 1
.Lsmod_skip:
    add64 r6, -1
    jne r6, 0, .Lsmod_loop
    xor64 r8, r7
    xor64 r9, r7
    sub64 r8, r7
    mov64 r0, 0
    jeq r7, 0, .Lsmod_done
    jne r8, 0, .Lsmod_done
    mov64 r0, 1
.Lsmod_done:
    add64 r9, r0
    ldxdw r1, [r10-40]
    stxdw [r1+0], r8
    stxdw [r1+8], r9
    ldxdw r6, [r10-8]
    ldxdw r7, [r10-16]
    ldxdw r8, [r10-24]
    ldxdw r9, [r10-32]
    exit
