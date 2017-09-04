// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/cfwl_message.h"

CFWL_Message::CFWL_Message()
    : m_pSrcTarget(nullptr), m_pDstTarget(nullptr), m_dwExtend(0) {}

CFWL_Message::~CFWL_Message() {}

std::unique_ptr<CFWL_Message> CFWL_Message::Clone() {
  return nullptr;
}

CFWL_MessageType CFWL_Message::GetClassID() const {
  return CFWL_MessageType::None;
}
