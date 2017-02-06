; Copyright (c) 2011 The Chromium Authors. All rights reserved.
; Use of this source code is governed by a BSD-style license that can be
; found in the LICENSE file.

%include "third_party/x86inc/x86inc.asm"

;
; This file uses MMX and SSE instructions.
;
  SECTION_TEXT
  CPU       MMX, SSE

; Use SSE instruction movntq can write faster.
%define MOVQ movntq

;
; extern "C" void ConvertYUVToRGB32Row_SSE(const uint8_t* y_buf,
;                                          const uint8_t* u_buf,
;                                          const uint8_t* v_buf,
;                                          uint8_t* rgb_buf,
;                                          ptrdiff_t width);
;                                          const int16_t* convert_table);
%define SYMBOL ConvertYUVToRGB32Row_SSE
%include "convert_yuv_to_rgb_mmx.inc"
