; Pong-style demo for the TCMIPS Atari 2600 emulator.
; Playfield-rendered: paddles + ball drawn via PF0/PF1/PF2 computed
; in the kernel per scanline (fits the 76-cycle line budget).
; 4K ROM at $F000. Non-reflected playfield; right half repeats left half.

  org $F000

VSYNC  = $00
VBLANK = $01
WSYNC  = $02
COLUPF = $08
COLUBK = $09
CTRLPF = $0A
PF0    = $0D
PF1    = $0E
PF2    = $0F
SWCHA  = $0280
INTIM  = $0284
TIM64T = $0296

lptop = $80
lpbot = $81
rptop = $82
rpbot = $83
bp    = $84   ; ball pixel x (0..159)
by    = $85   ; ball row (0..191)
vx    = $86   ; +/-2 px per frame
vy    = $87   ; +/-2 rows per frame
bm    = $88   ; ball playfield bit mask
bsel  = $89   ; 0=PF0 1=PF1 2=PF2
frame = $8A

reset:
  sei
  cld
  ldx #$FF
  txs
  lda #$00
  sta CTRLPF          ; non-reflected playfield
  lda #$0E
  sta COLUPF          ; white
  lda #$02
  sta COLUBK          ; dark gray
  lda #60
  sta lptop
  sta rptop
  lda #40
  sta bp
  lda #96
  sta by
  lda #2
  sta vx
  sta vy
  lda #0
  sta frame

frame_loop:
  ; ---- VSYNC pulse (3 lines) ----
  lda #2
  sta VSYNC
  sta WSYNC
  sta WSYNC
  sta WSYNC
  lda #0
  sta VSYNC
  ; ---- VBLANK on, run game logic; timer paces the kernel start ----
  lda #43             ; 37 lines * 76 cycles / 64
  sta TIM64T
  lda #2
  sta VBLANK
  jsr update
  jsr calc_ball
  lda lptop
  clc
  adc #20
  sta lpbot
  lda rptop
  clc
  adc #20
  sta rpbot
vb_wait:
  lda INTIM
  bne vb_wait
  sta WSYNC           ; align to the next line boundary
  ; ---- VBLANK off: 192 visible lines ----
  lda #0
  sta VBLANK
  jsr kernel
  ; ---- VBLANK on: overscan ----
  lda #2
  sta VBLANK
  inc frame
  ldx #29
opad:
  sta WSYNC
  dex
  bne opad
  jmp frame_loop

; ----------------------------------------------------------------------
update:
  lda SWCHA
  and #$08            ; P0 up (left paddle)
  bne lu_skip
  dec lptop
lu_skip:
  lda SWCHA
  and #$04            ; P0 down
  bne ld_skip
  inc lptop
ld_skip:
  lda SWCHA
  and #$80            ; P1 up (right paddle)
  bne ru_skip
  dec rptop
ru_skip:
  lda SWCHA
  and #$40            ; P1 down
  bne rd_skip
  inc rptop
rd_skip:
  ; clamp paddles to 0..171
  lda lptop
  bpl lc1
  lda #0
  sta lptop
lc1:
  cmp #172
  bcc lc2
  lda #171
  sta lptop
lc2:
  lda rptop
  bpl rc1
  lda #0
  sta rptop
rc1:
  cmp #172
  bcc rc2
  lda #171
  sta rptop
rc2:
  ; ---- move ball ----
  lda bp
  clc
  adc vx
  sta bp
  lda by
  clc
  adc vy
  sta by
  ; vertical bounce
  lda by
  cmp #2
  bcs vy_lo
  lda #2
  sta vy
vy_lo:
  lda by
  cmp #189
  bcc vy_done
  lda #$FE
  sta vy
vy_done:
  ; horizontal: right paddle at px 76-79
  lda vx
  bmi ball_left
  lda bp
  cmp #74
  bcc h_done
  lda by
  cmp rptop
  bcc serve
  cmp rpbot
  bcs serve
  lda #$FE
  sta vx
  jmp h_done
ball_left:
  ; left paddle at px 4-7
  lda bp
  cmp #10
  bcs h_done
  lda by
  cmp lptop
  bcc serve
  cmp lpbot
  bcs serve
  lda #2
  sta vx
  jmp h_done
serve:
  lda #40
  sta bp
  lda #96
  sta by
  lda #0
  sec
  sbc vx
  sta vx
  lda frame
  and #1
  beq sv_dn
  lda #$FE
  sta vy
  rts
sv_dn:
  lda #2
  sta vy
h_done:
  rts

; ----------------------------------------------------------------------
; calc_ball: bm = playfield bit for current ball pixel, bsel = byte
calc_ball:
  lda bp
  lsr
  lsr
  cmp #4
  bcc cb_pf0
  cmp #12
  bcc cb_pf1
  sec
  sbc #12
  tay
  lda #1                 ; PF2: bit0 = leftmost of its group
cb_l2:
  dey
  bmi cb_done
  asl
  jmp cb_l2
cb_pf1:
  sec
  sbc #4
  tay
  lda #$80               ; PF1: bit7 = leftmost of its group
cb_l1:
  dey
  bmi cb_done
  lsr
  jmp cb_l1
cb_pf0:
  tay
  lda #$80
cb_l0:
  dey
  bmi cb_done
  lsr
  jmp cb_l0
cb_done:
  sta bm
  lda bp
  lsr
  lsr
  cmp #4
  lda #0
  adc #0
  sta bsel
  lda bp
  lsr
  lsr
  cmp #12
  lda bsel
  adc #0
  sta bsel
  rts

; ----------------------------------------------------------------------
; ; ----------------------------------------------------------------------
; kernel: 192 visible lines. Structure: write the playfield registers
; FIRST (values computed during the previous line's tail, so all TIA
; stores land in hblank), then compute the NEXT line's values while the
; beam is drawing - no time pressure there.
pf0cur = $90
pf1cur = $91
pf2cur = $92
kernel:
  ldy #0
  lda #0            ; line 0 starts blank
  sta pf0cur
  sta pf1cur
  sta pf2cur
kloop:
  lda pf0cur
  sta PF0           ; hblank writes for current line
  lda pf1cur
  sta PF1
  lda pf2cur
  sta PF2
  ; ---- compute next line's register values ----
  lda #0
  sta pf0cur
  sta pf1cur
  sta pf2cur
  cpy lptop         ; left paddle rows [lptop, lpbot)
  bcc kc_l
  cpy lpbot
  bcs kc_l
  lda #$40          ; PF0 bit -> px 4-7
  sta pf0cur
kc_l:
  cpy rptop         ; right paddle rows [rptop, rpbot)
  bcc kc_r
  cpy rpbot
  bcs kc_r
  lda #$80          ; PF2 bit7 -> px 76-79 (PF2 is straight: bit0=px48)
  sta pf2cur
kc_r:
  cpy by            ; ball row?
  bne kc_nb
  lda bsel
  bne kc_b1
  lda bm            ; ball in PF0 segment
  ora pf0cur
  sta pf0cur
  jmp kc_nb
kc_b1:
  cmp #2
  bne kc_b2
  lda bm            ; ball in PF2 segment
  ora pf2cur
  sta pf2cur
  jmp kc_nb
kc_b2:
  lda bm            ; ball in PF1 segment
  ora pf1cur
  sta pf1cur
kc_nb:
  sta WSYNC         ; end of line
  iny
  cpy #192
  bne kloop
  rts

  org $FFFC
  .word reset
  .word reset