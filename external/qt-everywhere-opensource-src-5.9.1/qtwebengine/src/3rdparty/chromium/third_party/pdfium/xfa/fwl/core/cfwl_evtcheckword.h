// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_CFWL_EVTCHECKWORD_H_
#define XFA_FWL_CORE_CFWL_EVTCHECKWORD_H_

#include "xfa/fwl/core/cfwl_event.h"

class CFWL_EvtCheckWord : public CFWL_Event {
 public:
  CFWL_EvtCheckWord();
  ~CFWL_EvtCheckWord() override;

  CFWL_EventType GetClassID() const override;

  CFX_ByteString bsWord;
  bool bCheckWord;
};

#endif  // XFA_FWL_CORE_CFWL_EVTCHECKWORD_H_
