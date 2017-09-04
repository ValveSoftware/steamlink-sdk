// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_CFWL_EVTKEY_H_
#define XFA_FWL_CORE_CFWL_EVTKEY_H_

#include "xfa/fwl/core/cfwl_event.h"

class CFWL_EvtKey : public CFWL_Event {
 public:
  CFWL_EvtKey();
  ~CFWL_EvtKey() override;

  CFWL_EventType GetClassID() const override;

  uint32_t m_dwKeyCode;
  uint32_t m_dwFlags;
  FWL_KeyCommand m_dwCmd;
};

#endif  // XFA_FWL_CORE_CFWL_EVTKEY_H_
