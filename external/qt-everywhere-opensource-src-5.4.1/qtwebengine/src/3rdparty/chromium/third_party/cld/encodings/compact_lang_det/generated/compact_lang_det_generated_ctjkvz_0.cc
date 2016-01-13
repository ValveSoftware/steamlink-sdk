// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Created by utf8tablebuilder version 2.8
// See util/utf8/utf8statetable.h for usage
//
//  Maps properties of all codes from file:
//    compact_lang_det_generated_ctjkvz.txt
//  Accepts all other UTF-8 codes 0000..10FFFF
//  Space optimized
//
// ** ASSUMES INPUT IS STRUCTURALLY VALID UTF-8 **
//
//  Table offsets for byte 2-of-3 and byte 3-of-4 are
//  multiplied by 16; offsets for 3-of-3 and 4-of-4 are
//  relative +/-127 from previous state.

#include "encodings/compact_lang_det/win/cld_utf8statetable.h"


//  Entire table has 1 state block of 64 entries

static const unsigned int compact_lang_det_generated_ctjkvz_b1_STATE0 = 0;		// state[0]
static const unsigned int compact_lang_det_generated_ctjkvz_b1_STATE0_SIZE = 64;	// =[1]
static const unsigned int compact_lang_det_generated_ctjkvz_b1_TOTAL_SIZE = 64;
static const unsigned int compact_lang_det_generated_ctjkvz_b1_MAX_EXPAND_X4 = 0;
static const unsigned int compact_lang_det_generated_ctjkvz_b1_SHIFT = 6;
static const unsigned int compact_lang_det_generated_ctjkvz_b1_BYTES = 1;
static const unsigned int compact_lang_det_generated_ctjkvz_b1_LOSUB = 0x80808080;
static const unsigned int compact_lang_det_generated_ctjkvz_b1_HIADD = 0x00000000;

static const uint8 compact_lang_det_generated_ctjkvz_b1[] = {
// state[0] 0x000000 Byte 1 (row Ex offsets 16x small)
  0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,

};

// Remap base[0] = (del, add, string_offset)
static const RemapEntry compact_lang_det_generated_ctjkvz_b1_remap_base[] = {
{0,0,0} };

// Remap string[0]
static const unsigned char compact_lang_det_generated_ctjkvz_b1_remap_string[] = {
0 };

extern const UTF8PropObj compact_lang_det_generated_ctjkvz_b1_obj = {
  compact_lang_det_generated_ctjkvz_b1_STATE0,
  compact_lang_det_generated_ctjkvz_b1_STATE0_SIZE,
  compact_lang_det_generated_ctjkvz_b1_TOTAL_SIZE,
  compact_lang_det_generated_ctjkvz_b1_MAX_EXPAND_X4,
  compact_lang_det_generated_ctjkvz_b1_SHIFT,
  compact_lang_det_generated_ctjkvz_b1_BYTES,
  compact_lang_det_generated_ctjkvz_b1_LOSUB,
  compact_lang_det_generated_ctjkvz_b1_HIADD,
  compact_lang_det_generated_ctjkvz_b1,
  compact_lang_det_generated_ctjkvz_b1_remap_base,
  compact_lang_det_generated_ctjkvz_b1_remap_string,
  NULL
};
