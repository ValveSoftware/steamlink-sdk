// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_CFWL_EVTSCROLL_H_
#define XFA_FWL_CORE_CFWL_EVTSCROLL_H_

#include "xfa/fwl/core/cfwl_event.h"

enum class FWL_SCBCODE {
  None = 1,
  Min,
  Max,
  PageBackward,
  PageForward,
  StepBackward,
  StepForward,
  Pos,
  TrackPos,
  EndScroll,
};

class CFWL_EvtScroll : public CFWL_Event {
 public:
  CFWL_EvtScroll();
  ~CFWL_EvtScroll() override;

  CFWL_EventType GetClassID() const override;

  FWL_SCBCODE m_iScrollCode;
  FX_FLOAT m_fPos;
  bool* m_pRet;
};

#endif  // XFA_FWL_CORE_CFWL_EVTSCROLL_H_
