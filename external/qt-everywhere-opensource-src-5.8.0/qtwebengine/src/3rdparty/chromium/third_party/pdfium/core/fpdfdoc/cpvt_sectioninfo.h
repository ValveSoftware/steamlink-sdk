// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPVT_SECTIONINFO_H_
#define CORE_FPDFDOC_CPVT_SECTIONINFO_H_

#include "core/fpdfdoc/cpvt_floatrect.h"
#include "core/fpdfdoc/include/cpvt_secprops.h"
#include "core/fpdfdoc/include/cpvt_wordprops.h"

struct CPVT_SectionInfo {
  CPVT_SectionInfo()
      : rcSection(), nTotalLine(0), pSecProps(nullptr), pWordProps(nullptr) {}

  ~CPVT_SectionInfo() {
    delete pSecProps;
    delete pWordProps;
  }

  CPVT_SectionInfo(const CPVT_SectionInfo& other)
      : rcSection(), nTotalLine(0), pSecProps(nullptr), pWordProps(nullptr) {
    operator=(other);
  }

  void operator=(const CPVT_SectionInfo& other) {
    if (this == &other)
      return;

    rcSection = other.rcSection;
    nTotalLine = other.nTotalLine;
    if (other.pSecProps) {
      if (pSecProps)
        *pSecProps = *other.pSecProps;
      else
        pSecProps = new CPVT_SecProps(*other.pSecProps);
    }
    if (other.pWordProps) {
      if (pWordProps)
        *pWordProps = *other.pWordProps;
      else
        pWordProps = new CPVT_WordProps(*other.pWordProps);
    }
  }

  CPVT_FloatRect rcSection;
  int32_t nTotalLine;
  CPVT_SecProps* pSecProps;
  CPVT_WordProps* pWordProps;
};

#endif  // CORE_FPDFDOC_CPVT_SECTIONINFO_H_
