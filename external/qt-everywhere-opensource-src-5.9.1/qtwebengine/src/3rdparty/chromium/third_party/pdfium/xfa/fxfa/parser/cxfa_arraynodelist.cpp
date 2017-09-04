// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/xfa_object.h"

CXFA_ArrayNodeList::CXFA_ArrayNodeList(CXFA_Document* pDocument)
    : CXFA_NodeList(pDocument) {}

CXFA_ArrayNodeList::~CXFA_ArrayNodeList() {}

void CXFA_ArrayNodeList::SetArrayNodeList(const CXFA_NodeArray& srcArray) {
  if (srcArray.GetSize() > 0) {
    m_array.Copy(srcArray);
  }
}

int32_t CXFA_ArrayNodeList::GetLength() {
  return m_array.GetSize();
}

bool CXFA_ArrayNodeList::Append(CXFA_Node* pNode) {
  m_array.Add(pNode);
  return true;
}

bool CXFA_ArrayNodeList::Insert(CXFA_Node* pNewNode, CXFA_Node* pBeforeNode) {
  if (!pBeforeNode) {
    m_array.Add(pNewNode);
  } else {
    int32_t iSize = m_array.GetSize();
    for (int32_t i = 0; i < iSize; ++i) {
      if (m_array[i] == pBeforeNode) {
        m_array.InsertAt(i, pNewNode);
        break;
      }
    }
  }
  return true;
}

bool CXFA_ArrayNodeList::Remove(CXFA_Node* pNode) {
  int32_t iSize = m_array.GetSize();
  for (int32_t i = 0; i < iSize; ++i) {
    if (m_array[i] == pNode) {
      m_array.RemoveAt(i);
      break;
    }
  }
  return true;
}

CXFA_Node* CXFA_ArrayNodeList::Item(int32_t iIndex) {
  int32_t iSize = m_array.GetSize();
  if (iIndex >= 0 && iIndex < iSize) {
    return m_array[iIndex];
  }
  return nullptr;
}
