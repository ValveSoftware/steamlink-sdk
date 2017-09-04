// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_CFWL_EVTKILLFOCUS_H_
#define XFA_FWL_CORE_CFWL_EVTKILLFOCUS_H_

#include "xfa/fwl/core/cfwl_event.h"

class CFWL_EvtKillFocus : public CFWL_Event {
 public:
  CFWL_EvtKillFocus();
  ~CFWL_EvtKillFocus() override;

  CFWL_EventType GetClassID() const override;

  IFWL_Widget* m_pKillFocus;
};

#endif  // XFA_FWL_CORE_CFWL_EVTKILLFOCUS_H_
