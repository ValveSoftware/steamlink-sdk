// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_CFWL_LISTITEM_H_
#define XFA_FWL_CORE_CFWL_LISTITEM_H_

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_string.h"

class CFX_DIBitmap;

class CFWL_ListItem {
 public:
  CFWL_ListItem();
  ~CFWL_ListItem();

  CFX_RectF m_rtItem;
  uint32_t m_dwStates;
  uint32_t m_dwStyles;
  CFX_WideString m_wsText;
  CFX_DIBitmap* m_pDIB;
  void* m_pData;
  uint32_t m_dwCheckState;
  CFX_RectF m_rtCheckBox;
};

#endif  // XFA_FWL_CORE_CFWL_LISTITEM_H_
