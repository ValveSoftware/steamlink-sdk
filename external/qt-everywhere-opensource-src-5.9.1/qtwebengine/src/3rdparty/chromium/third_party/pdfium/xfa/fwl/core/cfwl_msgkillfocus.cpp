// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/cfwl_msgkillfocus.h"

#include "third_party/base/ptr_util.h"

CFWL_MsgKillFocus::CFWL_MsgKillFocus() {}

CFWL_MsgKillFocus::~CFWL_MsgKillFocus() {}

std::unique_ptr<CFWL_Message> CFWL_MsgKillFocus::Clone() {
  return pdfium::MakeUnique<CFWL_MsgKillFocus>(*this);
}

CFWL_MessageType CFWL_MsgKillFocus::GetClassID() const {
  return CFWL_MessageType::KillFocus;
}
