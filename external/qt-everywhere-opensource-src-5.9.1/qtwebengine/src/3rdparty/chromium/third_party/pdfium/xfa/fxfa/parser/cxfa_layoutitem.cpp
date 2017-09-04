// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_layoutitem.h"

#include "xfa/fxfa/app/xfa_ffnotify.h"
#include "xfa/fxfa/parser/cxfa_containerlayoutitem.h"
#include "xfa/fxfa/parser/cxfa_contentlayoutitem.h"

void XFA_ReleaseLayoutItem(CXFA_LayoutItem* pLayoutItem) {
  CXFA_LayoutItem* pNode = pLayoutItem->m_pFirstChild;
  CXFA_FFNotify* pNotify = pLayoutItem->m_pFormNode->GetDocument()->GetNotify();
  CXFA_LayoutProcessor* pDocLayout =
      pLayoutItem->m_pFormNode->GetDocument()->GetDocLayout();
  while (pNode) {
    CXFA_LayoutItem* pNext = pNode->m_pNextSibling;
    pNode->m_pParent = nullptr;
    pNotify->OnLayoutItemRemoving(pDocLayout,
                                  static_cast<CXFA_LayoutItem*>(pNode));
    XFA_ReleaseLayoutItem(pNode);
    pNode = pNext;
  }

  pNotify->OnLayoutItemRemoving(pDocLayout, pLayoutItem);
  if (pLayoutItem->m_pFormNode->GetElementType() == XFA_Element::PageArea) {
    pNotify->OnPageEvent(static_cast<CXFA_ContainerLayoutItem*>(pLayoutItem),
                         XFA_PAGEVIEWEVENT_PostRemoved);
  }
  delete pLayoutItem;
}

CXFA_LayoutItem::CXFA_LayoutItem(CXFA_Node* pNode, bool bIsContentLayoutItem)
    : m_pFormNode(pNode),
      m_pParent(nullptr),
      m_pNextSibling(nullptr),
      m_pFirstChild(nullptr),
      m_bIsContentLayoutItem(bIsContentLayoutItem) {}

CXFA_LayoutItem::~CXFA_LayoutItem() {}

CXFA_ContainerLayoutItem* CXFA_LayoutItem::AsContainerLayoutItem() {
  return IsContainerLayoutItem() ? static_cast<CXFA_ContainerLayoutItem*>(this)
                                 : nullptr;
}
CXFA_ContentLayoutItem* CXFA_LayoutItem::AsContentLayoutItem() {
  return IsContentLayoutItem() ? static_cast<CXFA_ContentLayoutItem*>(this)
                               : nullptr;
}
