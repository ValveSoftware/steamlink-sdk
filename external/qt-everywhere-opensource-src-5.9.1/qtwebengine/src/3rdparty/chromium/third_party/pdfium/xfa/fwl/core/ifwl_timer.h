// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_IFWL_TIMER_H_
#define XFA_FWL_CORE_IFWL_TIMER_H_

#include "core/fxcrt/fx_system.h"

class IFWL_AdapterTimerMgr;
class IFWL_TimerInfo;
class IFWL_Widget;

class IFWL_Timer {
 public:
  explicit IFWL_Timer(IFWL_Widget* parent) : m_pWidget(parent) {}
  virtual ~IFWL_Timer() {}

  virtual void Run(IFWL_TimerInfo* hTimer) = 0;
  IFWL_TimerInfo* StartTimer(uint32_t dwElapse, bool bImmediately);

 protected:
  IFWL_Widget* m_pWidget;  // Not owned.
};

class IFWL_TimerInfo {
 public:
  explicit IFWL_TimerInfo(IFWL_AdapterTimerMgr* mgr) : m_pMgr(mgr) {
    ASSERT(mgr);
  }
  virtual ~IFWL_TimerInfo() {}

  void StopTimer();

 private:
  IFWL_AdapterTimerMgr* m_pMgr;  // Not owned.
};

#endif  // XFA_FWL_CORE_IFWL_TIMER_H_
