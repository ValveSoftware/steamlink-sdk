// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/fpdfxfa/cxfa_fwladaptertimermgr.h"

#include <vector>

#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/fsdk_define.h"

std::vector<CFWL_TimerInfo*>* CXFA_FWLAdapterTimerMgr::s_TimerArray = nullptr;

void CXFA_FWLAdapterTimerMgr::Start(IFWL_Timer* pTimer,
                                    uint32_t dwElapse,
                                    bool bImmediately,
                                    IFWL_TimerInfo** pTimerInfo) {
  if (!m_pFormFillEnv)
    return;

  int32_t id_event = m_pFormFillEnv->SetTimer(dwElapse, TimerProc);
  if (!s_TimerArray)
    s_TimerArray = new std::vector<CFWL_TimerInfo*>;

  s_TimerArray->push_back(new CFWL_TimerInfo(this, id_event, pTimer));
  *pTimerInfo = s_TimerArray->back();
}

void CXFA_FWLAdapterTimerMgr::Stop(IFWL_TimerInfo* pTimerInfo) {
  if (!pTimerInfo || !m_pFormFillEnv)
    return;

  CFWL_TimerInfo* pInfo = static_cast<CFWL_TimerInfo*>(pTimerInfo);
  m_pFormFillEnv->KillTimer(pInfo->idEvent);
  if (s_TimerArray) {
    auto it = std::find(s_TimerArray->begin(), s_TimerArray->end(), pInfo);
    if (it != s_TimerArray->end()) {
      s_TimerArray->erase(it);
      delete pInfo;
    }
  }
}

// static
void CXFA_FWLAdapterTimerMgr::TimerProc(int32_t idEvent) {
  if (!s_TimerArray)
    return;

  for (CFWL_TimerInfo* pInfo : *s_TimerArray) {
    if (pInfo->idEvent == idEvent) {
      pInfo->pTimer->Run(pInfo);
      break;
    }
  }
}
