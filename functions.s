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
