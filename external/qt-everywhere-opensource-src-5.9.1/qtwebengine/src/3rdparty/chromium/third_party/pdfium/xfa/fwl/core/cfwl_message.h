// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_CFWL_MESSAGE_H_
#define XFA_FWL_CORE_CFWL_MESSAGE_H_

#include <memory>

#include "core/fxcrt/fx_basic.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "xfa/fwl/core/fwl_error.h"

enum class CFWL_MessageType {
  None = 0,
  Key,
  KillFocus,
  Mouse,
  MouseWheel,
  SetFocus
};

class IFWL_Widget;

class CFWL_Message {
 public:
  CFWL_Message();
  virtual ~CFWL_Message();

  virtual std::unique_ptr<CFWL_Message> Clone();
  virtual CFWL_MessageType GetClassID() const;

  IFWL_Widget* m_pSrcTarget;
  IFWL_Widget* m_pDstTarget;
  uint32_t m_dwExtend;
};


#endif  // XFA_FWL_CORE_CFWL_MESSAGE_H_
