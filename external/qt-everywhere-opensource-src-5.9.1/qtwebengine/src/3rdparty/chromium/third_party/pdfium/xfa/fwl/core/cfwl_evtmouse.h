// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_CFWL_EVTMOUSE_H_
#define XFA_FWL_CORE_CFWL_EVTMOUSE_H_

#include "xfa/fwl/core/cfwl_event.h"

class CFWL_EvtMouse : public CFWL_Event {
 public:
  CFWL_EvtMouse();
  ~CFWL_EvtMouse() override;

  CFWL_EventType GetClassID() const override;

  FX_FLOAT m_fx;
  FX_FLOAT m_fy;
  uint32_t m_dwFlags;
  FWL_MouseCommand m_dwCmd;
};

#endif  // XFA_FWL_CORE_CFWL_EVTMOUSE_H_
