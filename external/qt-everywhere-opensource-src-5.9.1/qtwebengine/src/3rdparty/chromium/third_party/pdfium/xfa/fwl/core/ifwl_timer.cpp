// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/ifwl_adaptertimermgr.h"
#include "xfa/fwl/core/ifwl_app.h"
#include "xfa/fwl/core/ifwl_timer.h"
#include "xfa/fwl/core/ifwl_widget.h"
#include "xfa/fxfa/xfa_ffapp.h"

IFWL_TimerInfo* IFWL_Timer::StartTimer(uint32_t dwElapse, bool bImmediately) {
  const IFWL_App* pApp = m_pWidget->GetOwnerApp();
  if (!pApp)
    return nullptr;

  CXFA_FFApp* pAdapterNative = pApp->GetAdapterNative();
  if (!pAdapterNative)
    return nullptr;

  IFWL_AdapterTimerMgr* pAdapterTimerMgr = pAdapterNative->GetTimerMgr();
  if (!pAdapterTimerMgr)
    return nullptr;

  IFWL_TimerInfo* pTimerInfo = nullptr;
  pAdapterTimerMgr->Start(this, dwElapse, bImmediately, &pTimerInfo);
  return pTimerInfo;
}

void IFWL_TimerInfo::StopTimer() {
  m_pMgr->Stop(this);
}
