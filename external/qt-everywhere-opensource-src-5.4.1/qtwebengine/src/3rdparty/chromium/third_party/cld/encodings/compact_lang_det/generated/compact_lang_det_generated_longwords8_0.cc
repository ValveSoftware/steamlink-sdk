// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Created by postproc-shortwords 1.6 on 2008-10-07 16:15:48
// From input file /tmp/input_10p_l8_sort.utf8
// See compact_lang_det.cc for usage
//
#include "encodings/compact_lang_det/cldutil.h"

// Suppressed:
//      az-Arab az-Cyrl ku-Latn tg-Arab za-Hani zzb-Latn zze-Latn zzh-Latn ru-Latn

// Remapped:
//      xxx-Latn=>ut-Latn sh-Latn=>hr-Latn sh-Cyrl=>sr-Cyrl

// ms/id probabilities leveled

static const int kLongWord8TableBuildDate = 20081007;    // yyyymmdd

COMPILE_ASSERT(MONTENEGRIN == 160, k_montenegrin_changed);
COMPILE_ASSERT(EXT_NUM_LANGUAGES == 209, k_ext_num_languages_changed);

static const int kLongWord8TableSize = 1;    // Bucket count
static const int kLongWord8TableKeyMask = 0xffffffff;    // Mask hash key

COMPILE_ASSERT(MONTENEGRIN == 160, k_montenegrin_changed);
COMPILE_ASSERT(EXT_NUM_LANGUAGES == 209, k_ext_num_languages_changed);

// Empty table
static const cld::IndirectProbBucket4 kLongWord8Table[kLongWord8TableSize] = {
  // key[4], words[4] in UTF-8
  // value[4]
  { {0x00000000,0x00000000,0x00000000,0x00000000}},	// [000] c
};

static const uint32 kLongWord8TableInd[1] = {
  // [0000]
  0x00000000, };

COMPILE_ASSERT(1 < (1 << 16), k_indirectbits_too_small);


extern const cld::CLDTableSummary kLongWord8Table_obj = {
  kLongWord8Table,
  kLongWord8TableInd,
  kLongWord8TableSize,
  arraysize(kLongWord8TableInd),
  kLongWord8TableKeyMask,
  kLongWord8TableBuildDate,
};

// End of generated tables
