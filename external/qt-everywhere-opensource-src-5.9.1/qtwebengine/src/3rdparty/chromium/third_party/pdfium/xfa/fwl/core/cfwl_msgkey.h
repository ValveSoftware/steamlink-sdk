// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_CFWL_MSGKEY_H_
#define XFA_FWL_CORE_CFWL_MSGKEY_H_

#include "xfa/fwl/core/cfwl_message.h"

enum class FWL_KeyCommand { KeyDown, KeyUp, Char };

class CFWL_MsgKey : public CFWL_Message {
 public:
  CFWL_MsgKey();
  ~CFWL_MsgKey() override;

  // CFWL_Message
  std::unique_ptr<CFWL_Message> Clone() override;
  CFWL_MessageType GetClassID() const override;

  uint32_t m_dwKeyCode;
  uint32_t m_dwFlags;
  FWL_KeyCommand m_dwCmd;
};

#endif  // XFA_FWL_CORE_CFWL_MSGKEY_H_
