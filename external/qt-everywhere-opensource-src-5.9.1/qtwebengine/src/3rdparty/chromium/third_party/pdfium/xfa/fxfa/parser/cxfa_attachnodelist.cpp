// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/xfa_object.h"

CXFA_AttachNodeList::CXFA_AttachNodeList(CXFA_Document* pDocument,
                                         CXFA_Node* pAttachNode)
    : CXFA_NodeList(pDocument) {
  m_pAttachNode = pAttachNode;
}

int32_t CXFA_AttachNodeList::GetLength() {
  return m_pAttachNode->CountChildren(
      XFA_Element::Unknown,
      m_pAttachNode->GetElementType() == XFA_Element::Subform);
}

bool CXFA_AttachNodeList::Append(CXFA_Node* pNode) {
  CXFA_Node* pParent = pNode->GetNodeItem(XFA_NODEITEM_Parent);
  if (pParent) {
    pParent->RemoveChild(pNode);
  }
  return m_pAttachNode->InsertChild(pNode);
}

bool CXFA_AttachNodeList::Insert(CXFA_Node* pNewNode, CXFA_Node* pBeforeNode) {
  CXFA_Node* pParent = pNewNode->GetNodeItem(XFA_NODEITEM_Parent);
  if (pParent) {
    pParent->RemoveChild(pNewNode);
  }
  return m_pAttachNode->InsertChild(pNewNode, pBeforeNode);
}

bool CXFA_AttachNodeList::Remove(CXFA_Node* pNode) {
  return m_pAttachNode->RemoveChild(pNode);
}

CXFA_Node* CXFA_AttachNodeList::Item(int32_t iIndex) {
  return m_pAttachNode->GetChild(
      iIndex, XFA_Element::Unknown,
      m_pAttachNode->GetElementType() == XFA_Element::Subform);
}
