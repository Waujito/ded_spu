input r0
call .factorial
print r0
halt

.factorial:
; if (r0 <= 1) return 1;
ldc r1 $1
cmp r0 r1
jmp.gt .fact_ret_deep
ldc r0 $1
ret

; return n * factorial(n - 1)
.fact_ret_deep:

; save n
pushr r0

; n -= 1
ldc r1 $1
sub r0 r0 r1

; r0 = factorial(n - 1)
call .factorial

; get n
popr r1

; n = n * factorial(n - 1)
mul r0 r1 r0
ret
