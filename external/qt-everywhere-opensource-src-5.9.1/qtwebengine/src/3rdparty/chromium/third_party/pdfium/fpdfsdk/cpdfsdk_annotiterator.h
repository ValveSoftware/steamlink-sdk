// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_CPDFSDK_ANNOTITERATOR_H_
#define FPDFSDK_CPDFSDK_ANNOTITERATOR_H_

#include <vector>

class CPDFSDK_Annot;
class CPDFSDK_PageView;

class CPDFSDK_AnnotIterator {
 public:
  CPDFSDK_AnnotIterator(CPDFSDK_PageView* pPageView, bool bReverse);
  ~CPDFSDK_AnnotIterator();

  CPDFSDK_Annot* Next();

 private:
  CPDFSDK_Annot* NextAnnot();
  CPDFSDK_Annot* PrevAnnot();

  std::vector<CPDFSDK_Annot*> m_iteratorAnnotList;
  const bool m_bReverse;
  std::size_t m_pos;
};

#endif  // FPDFSDK_CPDFSDK_ANNOTITERATOR_H_
