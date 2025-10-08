input r0	; a
input r1	; b
input r2	; c
mul r3 r1 r1	; b ^ 2
ldc r4 $4	; 4
mul r4 r4 r0	; 4 * a
mul r4 r4 r2	; 4 * a * c
sub r3 r3 r4	; b ^ 2 - 4 * a * c
sqrt r3 r3	; sqrt (D)
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
