// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FGAS_LAYOUT_FGAS_UNICODE_H_
#define XFA_FGAS_LAYOUT_FGAS_UNICODE_H_

#include "xfa/fgas/crt/fgas_utils.h"
#include "xfa/fgas/font/cfgas_fontmgr.h"

struct FX_TPO {
  int32_t index;
  int32_t pos;
};
typedef CFX_MassArrayTemplate<FX_TPO> CFX_TPOArray;

void FX_TEXTLAYOUT_PieceSort(CFX_TPOArray& tpos, int32_t iStart, int32_t iEnd);

typedef bool (*FX_AdjustCharDisplayPos)(FX_WCHAR wch,
                                        bool bMBCSCode,
                                        CFGAS_GEFont* pFont,
                                        FX_FLOAT fFontSize,
                                        bool bVertical,
                                        CFX_PointF& ptOffset);

#endif  // XFA_FGAS_LAYOUT_FGAS_UNICODE_H_
