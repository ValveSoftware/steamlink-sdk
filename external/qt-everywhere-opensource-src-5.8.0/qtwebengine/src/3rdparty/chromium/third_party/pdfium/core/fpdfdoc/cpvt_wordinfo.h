// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPVT_WORDINFO_H_
#define CORE_FPDFDOC_CPVT_WORDINFO_H_

#include "core/fpdfdoc/include/cpvt_wordprops.h"
#include "core/fxcrt/include/fx_system.h"

struct CPVT_WordInfo {
  CPVT_WordInfo()
      : Word(0),
        nCharset(FXFONT_ANSI_CHARSET),
        fWordX(0.0f),
        fWordY(0.0f),
        fWordTail(0.0f),
        nFontIndex(-1),
        pWordProps(nullptr) {}

  CPVT_WordInfo(uint16_t word,
                int32_t charset,
                int32_t fontIndex,
                CPVT_WordProps* pProps)
      : Word(word),
        nCharset(charset),
        fWordX(0.0f),
        fWordY(0.0f),
        fWordTail(0.0f),
        nFontIndex(fontIndex),
        pWordProps(pProps) {}

  CPVT_WordInfo(const CPVT_WordInfo& word)
      : Word(0),
        nCharset(FXFONT_ANSI_CHARSET),
        fWordX(0.0f),
        fWordY(0.0f),
        fWordTail(0.0f),
        nFontIndex(-1),
        pWordProps(nullptr) {
    operator=(word);
  }

  ~CPVT_WordInfo() { delete pWordProps; }

  void operator=(const CPVT_WordInfo& word) {
    if (this == &word)
      return;

    Word = word.Word;
    nCharset = word.nCharset;
    nFontIndex = word.nFontIndex;
    if (word.pWordProps) {
      if (pWordProps)
        *pWordProps = *word.pWordProps;
      else
        pWordProps = new CPVT_WordProps(*word.pWordProps);
    }
  }

  uint16_t Word;
  int32_t nCharset;
  FX_FLOAT fWordX;
  FX_FLOAT fWordY;
  FX_FLOAT fWordTail;
  int32_t nFontIndex;
  CPVT_WordProps* pWordProps;
};

#endif  // CORE_FPDFDOC_CPVT_WORDINFO_H_
