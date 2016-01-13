; Copyright (c) 2011 The Chromium Authors. All rights reserved.
; Use of this source code is governed by a BSD-style license that can be
; found in the LICENSE file.

%include "media/base/simd/media_export.asm"
%include "third_party/x86inc/x86inc.asm"

;
; This file uses MMX, SSE2 and instructions.
;
  SECTION_TEXT
  CPU       SSE2

; void ScaleYUVToRGB32Row_SSE2_X64(const uint8* y_buf,
;                                  const uint8* u_buf,
;                                  const uint8* v_buf,
;                                  uint8* rgb_buf,
;                                  ptrdiff_t width,
;                                  ptrdiff_t source_dx);
%define SYMBOL ScaleYUVToRGB32Row_SSE2_X64
  EXPORT    SYMBOL
  align     function_align

mangle(SYMBOL):
  %assign   stack_offset 0
  extern    mangle(kCoefficientsRgbY)

; Parameters are in the following order:
; 1. Y plane
; 2. U plane
; 3. V plane
; 4. ARGB frame
; 5. Width
; 6. Source dx
; 7. Convert table

PROLOGUE  7, 7, 3, Y, U, V, ARGB, WIDTH, SOURCE_DX, R1

%define     TABLEq   r10
%define     Xq       r11
%define     INDEXq   r12
%define     COMPq    R1q
%define     COMPd    R1d

  PUSH      r10
  PUSH      r11
  PUSH      r12

  mov TABLEq, R1q

  ; Set Xq index to 0.
  xor       Xq, Xq
  jmp       .scaleend

.scaleloop:
  ; Read UV pixels.
  mov       INDEXq, Xq
  sar       INDEXq, 17
  movzx     COMPd, BYTE [Uq + INDEXq]
  movq      xmm0, [TABLEq + 2048 + 8 * COMPq]
  movzx     COMPd, BYTE [Vq + INDEXq]
  movq      xmm1, [TABLEq + 4096 + 8 * COMPq]

  ; Read first Y pixel.
  lea       INDEXq, [Xq + SOURCE_DXq] ; INDEXq nows points to next pixel.
  sar       Xq, 16
  movzx     COMPd, BYTE [Yq + Xq]
  paddsw    xmm0, xmm1		      ; Hide a ADD after memory load.
  movq      xmm1, [TABLEq + 8 * COMPq]

  ;  Read next Y pixel.
  lea       Xq, [INDEXq + SOURCE_DXq] ; Xq now points to next pixel.
  sar       INDEXq, 16
  movzx     COMPd, BYTE [Yq + INDEXq]
  movq      xmm2, [TABLEq + 8 * COMPq]
  paddsw    xmm1, xmm0
  paddsw    xmm2, xmm0
  shufps    xmm1, xmm2, 0x44          ; Join two pixels into one XMM register
  psraw     xmm1, 6
  packuswb  xmm1, xmm1
  movq      QWORD [ARGBq], xmm1
  add       ARGBq, 8

.scaleend:
  sub       WIDTHq, 2
  jns       .scaleloop

  and       WIDTHq, 1                 ; odd number of pixels?
  jz        .scaledone

  ; Read U V components.
  mov       INDEXq, Xq
  sar       INDEXq, 17
  movzx     COMPd, BYTE [Uq + INDEXq]
  movq      xmm0, [TABLEq + 2048 + 8 * COMPq]
  movzx     COMPd, BYTE [Vq + INDEXq]
  movq      xmm1, [TABLEq + 4096 + 8 * COMPq]
  paddsw    xmm0, xmm1

  ; Read one Y component.
  mov       INDEXq, Xq
  sar       INDEXq, 16
  movzx     COMPd, BYTE [Yq + INDEXq]
  movq      xmm1, [TABLEq + 8 * COMPq]
  paddsw    xmm1, xmm0
  psraw     xmm1, 6
  packuswb  xmm1, xmm1
  movd      DWORD [ARGBq], xmm1

.scaledone:
  POP       r12
  POP       r11
  POP       r10
  RET
