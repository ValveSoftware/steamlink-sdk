// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_FPDFXFA_CXFA_FWLADAPTERTIMERMGR_H_
#define FPDFSDK_FPDFXFA_CXFA_FWLADAPTERTIMERMGR_H_

#include <vector>

#include "fpdfsdk/fpdfxfa/cpdfxfa_context.h"
#include "xfa/fwl/core/ifwl_adaptertimermgr.h"

struct CFWL_TimerInfo;

class CXFA_FWLAdapterTimerMgr : public IFWL_AdapterTimerMgr {
 public:
  CXFA_FWLAdapterTimerMgr(CPDFSDK_FormFillEnvironment* pFormFillEnv)
      : m_pFormFillEnv(pFormFillEnv) {}

  void Start(IFWL_Timer* pTimer,
             uint32_t dwElapse,
             bool bImmediately,
             IFWL_TimerInfo** pTimerInfo) override;
  void Stop(IFWL_TimerInfo* pTimerInfo) override;

 protected:
  static void TimerProc(int32_t idEvent);

  static std::vector<CFWL_TimerInfo*>* s_TimerArray;
  CPDFSDK_FormFillEnvironment* const m_pFormFillEnv;
};

struct CFWL_TimerInfo : public IFWL_TimerInfo {
  CFWL_TimerInfo(IFWL_AdapterTimerMgr* mgr, int32_t event, IFWL_Timer* timer)
      : IFWL_TimerInfo(mgr), idEvent(event), pTimer(timer) {}

  int32_t idEvent;
  IFWL_Timer* pTimer;
};

#endif  // FPDFSDK_FPDFXFA_CXFA_FWLADAPTERTIMERMGR_H_
