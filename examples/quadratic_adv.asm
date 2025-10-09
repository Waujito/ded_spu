input r0	; a
input r1	; b
input r2	; c

; Tests for linear equation
ldc r6 $0
cmp r0 r6
jmp.eq .linear_equation

mul r3 r1 r1	; b ^ 2
ldc r4 $4	; 4
mul r4 r4 r0	; 4 * a
mul r4 r4 r2	; 4 * a * c
sub r3 r3 r4	; b ^ 2 - 4 * a * c

; Tests for negative discriminant
ldc r6 $0
cmp r3 r6
jmp.l .negative_discriminant

sqrt r3 r3	; sqrt (D)
ldc r6 $2
print r6
ldc r4 $0
sub r4 r4 r1	; -b
ldc r6 $2	; 2
mul r6 r0 r6	; 2 * a
sub r5 r4 r3	; -b + sqrt(D)
div r5 r5 r6	; (-b + sqrt(d)) / (s * a)
print r5
add r5 r4 r3	; -b - sqrt(D)
div r5 r5 r6	; (-b - sqrt(d)) / (s * a)
print r5
halt
.linear_equation:
ldc r6 $-1
print r6
halt
.negative_discriminant:
ldc r6 $0
print r6
halt
