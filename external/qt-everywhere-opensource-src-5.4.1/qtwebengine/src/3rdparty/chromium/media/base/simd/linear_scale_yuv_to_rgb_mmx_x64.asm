; Copyright (c) 2011 The Chromium Authors. All rights reserved.
; Use of this source code is governed by a BSD-style license that can be
; found in the LICENSE file.

%include "media/base/simd/media_export.asm"
%include "third_party/x86inc/x86inc.asm"

;
; This file uses MMX instructions.
;
  SECTION_TEXT
  CPU       MMX

;void LinearScaleYUVToRGB32Row_MMX_X64(const uint8* y_buf,
;                                      const uint8* u_buf,
;                                      const uint8* v_buf,
;                                      uint8* rgb_buf,
;                                      ptrdiff_t width,
;                                      ptrdiff_t source_dx);
%define SYMBOL LinearScaleYUVToRGB32Row_MMX_X64
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
; 7. Conversion lookup table

PROLOGUE  7, 7, 3, Y, U, V, ARGB, WIDTH, SOURCE_DX, R1

%define     TABLEq     r10
%define     Xq         r11
%define     INDEXq     r12
%define     COMPRd     r13d
%define     COMPRq     r13
%define     FRACTIONq  r14
%define     COMPL      R1
%define     COMPLq     R1q
%define     COMPLd     R1d

  PUSH      TABLEq
  PUSH      Xq
  PUSH      INDEXq
  PUSH      COMPRq
  PUSH      FRACTIONq

%macro EPILOGUE 0
  POP       FRACTIONq
  POP       COMPRq
  POP       INDEXq
  POP       Xq
  POP       TABLEq
%endmacro

  mov       TABLEq, R1q

  imul      WIDTHq, SOURCE_DXq           ; source_width = width * source_dx
  xor       Xq, Xq                       ; x = 0
  cmp       SOURCE_DXq, 0x20000
  jl        .lscaleend
  mov       Xq, 0x8000                   ; x = 0.5 for 1/2 or less
  jmp       .lscaleend

.lscaleloop:
  ; Interpolate U
  mov       INDEXq, Xq
  sar       INDEXq, 0x11
  movzx     COMPLd, BYTE [Uq + INDEXq]
  movzx     COMPRd, BYTE [Uq + INDEXq + 1]
  mov       FRACTIONq, Xq
  and       FRACTIONq, 0x1fffe
  imul      COMPRq, FRACTIONq
  xor       FRACTIONq, 0x1fffe
  imul      COMPLq, FRACTIONq
  add       COMPLq, COMPRq
  shr       COMPLq, 17
  movq      mm0, [TABLEq + 2048 + 8 * COMPLq]

  ; Interpolate V
  movzx     COMPLd, BYTE [Vq + INDEXq]
  movzx     COMPRd, BYTE [Vq + INDEXq + 1]
  ; Trick here to imul COMPL first then COMPR.
  ; Saves two instruction. :)
  imul      COMPLq, FRACTIONq
  xor       FRACTIONq, 0x1fffe
  imul      COMPRq, FRACTIONq
  add       COMPLq, COMPRq
  shr       COMPLq, 17
  paddsw    mm0, [TABLEq + 4096 + 8 * COMPLq]

  ; Interpolate first Y1.
  lea       INDEXq, [Xq + SOURCE_DXq]   ; INDEXq now points to next pixel.
                                        ; Xq points to current pixel.
  mov       FRACTIONq, Xq
  sar       Xq, 0x10
  movzx     COMPLd, BYTE [Yq + Xq]
  movzx     COMPRd, BYTE [Yq + Xq + 1]
  and       FRACTIONq, 0xffff
  imul      COMPRq, FRACTIONq
  xor       FRACTIONq, 0xffff
  imul      COMPLq, FRACTIONq
  add       COMPLq, COMPRq
  shr       COMPLq, 16
  movq      mm1, [TABLEq + 8 * COMPLq]

  ; Interpolate Y2 if available.
  cmp       INDEXq, WIDTHq
  jge       .lscalelastpixel

  lea       Xq, [INDEXq + SOURCE_DXq]    ; Xq points to next pixel.
                                         ; INDEXq points to current pixel.
  mov       FRACTIONq, INDEXq
  sar       INDEXq, 0x10
  movzx     COMPLd, BYTE [Yq + INDEXq]
  movzx     COMPRd, BYTE [Yq + INDEXq + 1]
  and       FRACTIONq, 0xffff
  imul      COMPRq, FRACTIONq
  xor       FRACTIONq, 0xffff
  imul      COMPLq, FRACTIONq
  add       COMPLq, COMPRq
  shr       COMPLq, 16
  movq      mm2, [TABLEq + 8 * COMPLq]

  paddsw    mm1, mm0
  paddsw    mm2, mm0
  psraw     mm1, 0x6
  psraw     mm2, 0x6
  packuswb  mm1, mm2
  movntq    [ARGBq], mm1
  add       ARGBq, 0x8

.lscaleend:
  cmp       Xq, WIDTHq
  jl        .lscaleloop
  jmp       .epilogue

.lscalelastpixel:
  paddsw    mm1, mm0
  psraw     mm1, 6
  packuswb  mm1, mm1
  movd      [ARGBq], mm1

.epilogue
  EPILOGUE
  RET
