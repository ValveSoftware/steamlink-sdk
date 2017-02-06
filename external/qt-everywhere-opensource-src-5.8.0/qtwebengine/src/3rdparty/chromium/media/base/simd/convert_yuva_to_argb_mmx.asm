; Copyright (c) 2011 The Chromium Authors. All rights reserved.
; Use of this source code is governed by a BSD-style license that can be
; found in the LICENSE file.

%include "third_party/x86inc/x86inc.asm"

;
; This file uses MMX instructions.
;
  SECTION_TEXT
  CPU       MMX

; Use movq to save the output.
%define MOVQ movq

; extern "C" void ConvertYUVAToARGBRow_MMX(const uint8_t* y_buf,
;                                          const uint8_t* u_buf,
;                                          const uint8_t* v_buf,
;                                          const uint8_t* a_buf,
;                                          uint8_t* rgb_buf,
;                                          ptrdiff_t width);
;                                          const int16_t* convert_table);
%define SYMBOL ConvertYUVAToARGBRow_MMX
%include "convert_yuva_to_argb_mmx.inc"
