; r3 is offset of vram in ram
ldc r3 $10

pushr r3
mov r0 r3
; fill area on offset r0 with zeros
call .fill_zeros

popr r3
pushr r3
draw r3

mov r0 r3
; fill area on offset r0 with circle
call .fill_circle

popr r3

draw r3
halt

; r0 = vram offset
; fills screen with circle
.fill_circle:
scrhw r5 r6
print r5
print r6

; r7 and r8 are counters of y and x
ldc r7 $0
.fill_circle_y:

ldc r8 $0
.fill_circle_x:

; prepare arguments for function
; save needed registers on stack
pushr r5
pushr r6
pushr r7
pushr r8
pushr r0

; prepare arguments for function
mov r0 r7 ; counter on y
mov r1 r8 ; counter on x
popr r2   ; vram offset
pushr r2

pushr r0
pushr r1
pushr r2

; move center of circle to the center of screen
ldc r3 $2
div r5 r5 r3
div r6 r6 r3
sub r0 r0 r5
sub r1 r1 r6

call .circle_equation

; circle_equation returns color of circle point
mov r3 r0
popr r2
popr r1
popr r0

; plot the point
call .point_on_idx

popr r0
popr r8
popr r7
popr r6
popr r5

;increment counters

ldc r9 $1
add r8 r8 r9
cmp r8 r6
jmp.lt .fill_circle_x

add r7 r7 r9
cmp r7 r5
jmp.lt .fill_circle_y

ret


; r0 = x, r1 = y, returns color in r0
.circle_equation:
mul r0 r0 r0	; x^2
mul r1 r1 r1 	; y^2
add r0 r1 r0 	; x^2 + y^2
ldc r1 $32   	; r^2
cmp r0 r1	; x^2 + y^2 <= r^2
jmp.gt .circle_zero
ldc r0 $1	; yes - return 1
jmp .circle_ret
.circle_zero:
ldc r0 $0	; no - return 0
.circle_ret:
ret


; r0 = mem_base_addr
.fill_zeros:
scrhw r5 r6
print r5
print r6

ldc r7 $0
.cycle1:

ldc r8 $0
.cycle2:

pushr r5
pushr r6
pushr r7
pushr r8
pushr r0

mov r0 r7
mov r1 r8
popr r2	; vram offset
pushr r2
ldc r3 $0
call .point_on_idx

popr r0
popr r8
popr r7
popr r6
popr r5

ldc r9 $1
add r8 r8 r9
cmp r8 r6
jmp.lt .cycle2

add r7 r7 r9
cmp r7 r5
jmp.lt .cycle1

ret

; r0 = ypos, r1 = xpos, r2 = mem_base_addr, r3 (character) = point_data, 
.point_on_idx:
; r5 = height
scrhw r5 r6
; r5 = height * ypos
mul r5 r5 r0
; r5 = height * ypos + xpos
add r5 r5 r1
ldc r6 $3
; r6 = r5 >> 3
; r6 = r5 / 8
; In fact, r6 now stores address in memory
; Of the 8-byte word with 8 colors
shr r6 r5 r6
; Add base memory address for indexing
add r6 r6 r2
; r7 = ram[r6]
ldm r6 r7
ldc r8 $0x07
; r8 = r5 % 8
; Index inside the 8-byte word with 8 colors
and r8 r5 r8
ldc r9 $3
; r8 = r8 * 8
shl r8 r8 r9
; r9 = mask for 8-bit color
ldc r9 $0xff
; shift mask to the color position
shl r9 r9 r8
not r9 r9
; clear out old color in 8-byte word with mask
and r7 r7 r9
; shift the new color
shl r3 r3 r8
; mask out the new color
not r9 r9
and r3 r3 r9
; put new color in old colors word
or r7 r3 r7
; write new color to memory
stm r6 r7
ret
