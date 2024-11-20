@functions.s

.global increaseScore
increaseScore:
    cmp r1, #8
    beq .boss
    cmp r1, #16
    beq .enemy1
    @enemy2
    add r0, r0, #20
    b .end
.boss:
    add r0, r0, #150
    add r0, r0, #100
    b .end
.enemy1:
    add r0, r0, #15
.end:
    mov pc, lr


.global getOffsetForNum
getOffsetForNum:
    mov r1, #0
    mov r2, #44
.top:
    cmp r1, r0
    beq .endOff
    add r2, r2, #2
    add r1, r1, #1
    b .top
.endOff:
    mov r0, r2
    mov pc, lr


