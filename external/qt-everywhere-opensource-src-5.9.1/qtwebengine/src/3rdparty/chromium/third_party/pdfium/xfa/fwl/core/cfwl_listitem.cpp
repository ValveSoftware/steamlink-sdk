// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/cfwl_listitem.h"

CFWL_ListItem::CFWL_ListItem()
    : m_dwStates(0),
      m_wsText(L""),
      m_pDIB(nullptr),
      m_pData(nullptr),
      m_dwCheckState(0) {
  m_rtCheckBox.Reset();
  m_rtItem.Reset();
}

CFWL_ListItem::~CFWL_ListItem() {}
