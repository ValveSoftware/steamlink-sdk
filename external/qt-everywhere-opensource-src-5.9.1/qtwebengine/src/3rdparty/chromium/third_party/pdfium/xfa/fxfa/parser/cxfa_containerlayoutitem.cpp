// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_containerlayoutitem.h"

#include "xfa/fxfa/parser/cxfa_layoutpagemgr.h"
#include "xfa/fxfa/parser/cxfa_layoutprocessor.h"
#include "xfa/fxfa/parser/cxfa_measurement.h"

CXFA_ContainerLayoutItem::CXFA_ContainerLayoutItem(CXFA_Node* pNode)
    : CXFA_LayoutItem(pNode, false), m_pOldSubform(nullptr) {}

CXFA_LayoutProcessor* CXFA_ContainerLayoutItem::GetLayout() const {
  return m_pFormNode->GetDocument()->GetLayoutProcessor();
}

int32_t CXFA_ContainerLayoutItem::GetPageIndex() const {
  return m_pFormNode->GetDocument()
      ->GetLayoutProcessor()
      ->GetLayoutPageMgr()
      ->GetPageIndex(this);
}

CFX_SizeF CXFA_ContainerLayoutItem::GetPageSize() const {
  CFX_SizeF size;
  CXFA_Node* pMedium = m_pFormNode->GetFirstChildByClass(XFA_Element::Medium);
  if (!pMedium)
    return size;

  size = CFX_SizeF(pMedium->GetMeasure(XFA_ATTRIBUTE_Short).ToUnit(XFA_UNIT_Pt),
                   pMedium->GetMeasure(XFA_ATTRIBUTE_Long).ToUnit(XFA_UNIT_Pt));
  if (pMedium->GetEnum(XFA_ATTRIBUTE_Orientation) ==
      XFA_ATTRIBUTEENUM_Landscape) {
    size = CFX_SizeF(size.y, size.x);
  }
  return size;
}

CXFA_Node* CXFA_ContainerLayoutItem::GetMasterPage() const {
  return m_pFormNode;
}
