; CATCH — a minimal original game for this CHIP-8 emulator.
; A ball falls from the top; move the paddle under it to catch it.
; Catch = short beep + a point (score at the top right), miss = long
; beep. Endless. Left/right = hex keys 7/9 (QWERTY A/D).
;
; Registers:
;   V0 paddle x (0..56, step 8)   V1 ball x      V2 ball y
;   V4 score                      V5, V6 temps   V7, V8, VC saves
;   V9 = 56 (tens digit x)  VA = 0 (digit y)  VB = 60  VD = 30 (paddle y)

start:
    LD V0, 24        ; 6018  paddle starts left of center
    LD V4, 0         ; 6400  score
    LD V9, 56        ; 6938
    LD VA, 0         ; 6A00
    LD VB, 60        ; 6B3C
    LD VD, 30        ; 6D1E  paddle row
new_ball:
    RND V1, 0x38     ; C138  ball x = 0,8,...,56 (aligned with the paddle)
    LD V2, 0         ; 6200  ball starts at the top
main:
    LD V6, 9         ; 6609  right key
    SKP V6           ; E69E
    JP check_left    ; held: fall through to the move
    LD V6, 56        ; 6638  rightmost paddle position
    SE V0, V6        ; 5060  already there? skip the move
    ADD V0, 8        ; 7008
check_left:
    LD V6, 7         ; 6607  left key
    SKP V6           ; E69E
    JP timing        ; not held: skip the move
    LD V6, 0         ; 6600
    SE V0, V6        ; 5060  already at the left edge? skip
    ADD V0, 0xF8     ; 70F8  V0 -= 8 (wraps, guarded above)
timing:
    LD V5, DT        ; F507
    SE V5, 0         ; 3500  DT expired? skip the wait
    JP draw          ; still waiting: draw this frame
fall:
    ADD V2, 1        ; 7201  one row down
    LD V5, 6         ; 6506
    LD DT, V5        ; F515  next row in ~100 ms
    LD V5, 30        ; 651E
    SE V2, V5        ; 5250  reached the paddle row?
    JP draw          ; no: keep falling
    SE V0, V1        ; 5010  paddle aligned with the ball?
    JP miss          ; no: it got away
catch:
    ADD V4, 1        ; 7401  score!
    LD V5, 4         ; 6504
    LD ST, V5        ; F518  short beep
    JP new_ball      ; 12..
miss:
    LD V5, 15        ; 650F
    LD ST, V5        ; F518  long beep
    JP new_ball      ; 12..
draw:
    CLS              ; 00E0
    ; score: BCD V4 -> tens/ones digits at (56,0) and (60,0).
    ; FX65 starts at V0, so V0/V1/V2 are saved first.
    LD V7, V0        ; 8700  save paddle x
    LD V8, V1        ; 8810  save ball x
    LD VC, V2        ; 8C20  save ball y
    LD I, 0x300      ; A300
    BCD V4           ; F433  hundreds/tens/ones at 0x300..0x302
    LD I, 0x301      ; A301
    LD V2, [I]       ; F265  V0=tens, V1=ones (V2 clobbered, restored below)
    LD F, V0         ; F029  font sprite for the tens digit
    DRW V9, VA, 1    ; D9A1  at (56,0)
    LD F, V1         ; F129
    DRW VB, VA, 1    ; DBA1  at (60,0)
    LD V0, V7        ; 8070  restore paddle x
    LD V1, V8        ; 8180  restore ball x
    LD V2, VC        ; 82C0  restore ball y
    LD I, paddle     ; A2..
    DRW V0, VD, 1    ; D0D1  paddle at (V0,30)
    LD I, ball       ; A2..
    DRW V1, V2, 1    ; D121  ball at (V1,V2)
    JP main          ; 12..

paddle:
    .byte 0xFF       ; 8 pixels wide
ball:
    .byte 0xC0       ; 2 pixels wide
