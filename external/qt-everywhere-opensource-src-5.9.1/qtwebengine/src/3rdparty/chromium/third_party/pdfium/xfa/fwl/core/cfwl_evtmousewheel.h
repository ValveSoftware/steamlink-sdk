// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_CFWL_EVTMOUSEWHEEL_H_
#define XFA_FWL_CORE_CFWL_EVTMOUSEWHEEL_H_

#include "xfa/fwl/core/cfwl_event.h"

class CFWL_EvtMouseWheel : public CFWL_Event {
 public:
  CFWL_EvtMouseWheel();
  ~CFWL_EvtMouseWheel() override;

  CFWL_EventType GetClassID() const override;

  FX_FLOAT m_fx;
  FX_FLOAT m_fy;
  FX_FLOAT m_fDeltaX;
  FX_FLOAT m_fDeltaY;
  uint32_t m_dwFlags;
};

#endif  // XFA_FWL_CORE_CFWL_EVTMOUSEWHEEL_H_
