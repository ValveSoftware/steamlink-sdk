// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/cfwl_msgkey.h"

#include "third_party/base/ptr_util.h"

CFWL_MsgKey::CFWL_MsgKey() {}

CFWL_MsgKey::~CFWL_MsgKey() {}

std::unique_ptr<CFWL_Message> CFWL_MsgKey::Clone() {
  return pdfium::MakeUnique<CFWL_MsgKey>(*this);
}

CFWL_MessageType CFWL_MsgKey::GetClassID() const {
  return CFWL_MessageType::Key;
}
