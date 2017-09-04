// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_contentlayoutitem.h"

CXFA_ContentLayoutItem::CXFA_ContentLayoutItem(CXFA_Node* pNode)
    : CXFA_LayoutItem(pNode, true),
      m_pPrev(nullptr),
      m_pNext(nullptr),
      m_dwStatus(0) {}

CXFA_ContentLayoutItem::~CXFA_ContentLayoutItem() {
  if (m_pFormNode->GetUserData(XFA_LAYOUTITEMKEY) == this)
    m_pFormNode->SetUserData(XFA_LAYOUTITEMKEY, nullptr);
}
