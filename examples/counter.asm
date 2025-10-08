ldc r0 $1
ldc r1 $100
ldc r2 $0
.loop:
cmp r2 r1
jmp.eq .exit
add r2 r2 r0
print r2
jmp .loop
.exit:
dump
halt
