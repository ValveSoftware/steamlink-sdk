// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/cpdfsdk_annotiterator.h"

#include <algorithm>

#include "fpdfsdk/cpdfsdk_annot.h"
#include "fpdfsdk/cpdfsdk_pageview.h"

CPDFSDK_AnnotIterator::CPDFSDK_AnnotIterator(CPDFSDK_PageView* pPageView,
                                             bool bReverse)
    : m_bReverse(bReverse), m_pos(0) {
  const std::vector<CPDFSDK_Annot*>& annots = pPageView->GetAnnotList();
  m_iteratorAnnotList.insert(m_iteratorAnnotList.begin(), annots.rbegin(),
                             annots.rend());
  std::stable_sort(m_iteratorAnnotList.begin(), m_iteratorAnnotList.end(),
                   [](CPDFSDK_Annot* p1, CPDFSDK_Annot* p2) {
                     return p1->GetLayoutOrder() < p2->GetLayoutOrder();
                   });

  CPDFSDK_Annot* pTopMostAnnot = pPageView->GetFocusAnnot();
  if (!pTopMostAnnot)
    return;

  auto it = std::find(m_iteratorAnnotList.begin(), m_iteratorAnnotList.end(),
                      pTopMostAnnot);
  if (it != m_iteratorAnnotList.end()) {
    CPDFSDK_Annot* pReaderAnnot = *it;
    m_iteratorAnnotList.erase(it);
    m_iteratorAnnotList.insert(m_iteratorAnnotList.begin(), pReaderAnnot);
  }
}

CPDFSDK_AnnotIterator::~CPDFSDK_AnnotIterator() {}

CPDFSDK_Annot* CPDFSDK_AnnotIterator::NextAnnot() {
  if (m_pos < m_iteratorAnnotList.size())
    return m_iteratorAnnotList[m_pos++];
  return nullptr;
}

CPDFSDK_Annot* CPDFSDK_AnnotIterator::PrevAnnot() {
  if (m_pos < m_iteratorAnnotList.size())
    return m_iteratorAnnotList[m_iteratorAnnotList.size() - ++m_pos];
  return nullptr;
}

CPDFSDK_Annot* CPDFSDK_AnnotIterator::Next() {
  return m_bReverse ? PrevAnnot() : NextAnnot();
}
