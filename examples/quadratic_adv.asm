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
jmp.lt .no_solutions

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
cmp r1 r6	; b == 0
jmp.eq .linear_no_x
ldc r6 $1	; x = -c / b
print r6
ldc r6 $0
sub r6 r6 r2
div r6 r6 r1
print r6
halt

.linear_no_x:	; if b == 0, if c == 0 x is any number, 
	 	; if c != 0 here is no real solutions
cmp r2 r6
jmp.neq .no_solutions
ldc r6 $1
print r6
ldc r6 $0
print r6
halt

.no_solutions:
ldc r6 $0
print r6
halt
