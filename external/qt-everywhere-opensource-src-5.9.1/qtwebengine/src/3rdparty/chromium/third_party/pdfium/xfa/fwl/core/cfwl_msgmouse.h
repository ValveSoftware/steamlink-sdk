// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_CFWL_MSGMOUSE_H_
#define XFA_FWL_CORE_CFWL_MSGMOUSE_H_

#include "xfa/fwl/core/cfwl_message.h"

enum class FWL_MouseCommand {
  LeftButtonDown,
  LeftButtonUp,
  LeftButtonDblClk,
  RightButtonDown,
  RightButtonUp,
  RightButtonDblClk,
  Move,
  Enter,
  Leave,
  Hover
};

class CFWL_MsgMouse : public CFWL_Message {
 public:
  CFWL_MsgMouse();
  ~CFWL_MsgMouse() override;

  // CFWL_Message
  std::unique_ptr<CFWL_Message> Clone() override;
  CFWL_MessageType GetClassID() const override;

  FX_FLOAT m_fx;
  FX_FLOAT m_fy;
  uint32_t m_dwFlags;
  FWL_MouseCommand m_dwCmd;
};

#endif  // XFA_FWL_CORE_CFWL_MSGMOUSE_H_
