// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Created by postproc-shortwords 1.8 on 2009-03-22 11:11:34
// From input file /tmp/good_quad_input4567_sort.utf8
// See compact_lang_det.cc for usage
//
#include "encodings/compact_lang_det/cldutil.h"

// Suppressed:
//      ms-Latn gl-Latn mt-Latn af-Latn eu-Latn mk-Cyrl fa-Arab

// Remapped:
//      xxx-Latn=>ut-Latn sh-Latn=>hr-Latn sh-Cyrl=>sr-Cyrl

// ms/id probabilities leveled

static const int kQuadTableBuildDate = 20090322;    // yyyymmdd

COMPILE_ASSERT(MONTENEGRIN == 160, k_montenegrin_changed);
COMPILE_ASSERT(EXT_NUM_LANGUAGES == 209, k_ext_num_languages_changed);


static const int kQuadTableSize = 1;    // Bucket count
static const int kQuadTableKeyMask = 0xffffffff;    // Mask hash key


// Empty table
static const cld::IndirectProbBucket4 kQuadTable[kQuadTableSize] = {
  // key[4], words[4] in UTF-8
  // value[4]
  { {0x00000000,0x00000000,0x00000000,0x00000000}},  // [000] c
};

static const uint32 kQuadTableInd[1] = {
  // [0000]
  0x00000000, };


extern const cld::CLDTableSummary kQuadTable_obj = {
  kQuadTable,
  kQuadTableInd,
  kQuadTableSize,
  arraysize(kQuadTableInd),
  kQuadTableKeyMask,
  kQuadTableBuildDate,
};

// End of generated tables
