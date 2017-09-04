// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_IFWL_CARET_H_
#define XFA_FWL_CORE_IFWL_CARET_H_

#include <memory>

#include "xfa/fwl/core/ifwl_timer.h"
#include "xfa/fwl/core/ifwl_widget.h"
#include "xfa/fxgraphics/cfx_color.h"

class CFWL_WidgetProperties;
class IFWL_Widget;

#define FWL_STATE_CAT_HightLight 1

class IFWL_Caret : public IFWL_Widget {
 public:
  IFWL_Caret(const IFWL_App* app,
             std::unique_ptr<CFWL_WidgetProperties> properties,
             IFWL_Widget* pOuter);
  ~IFWL_Caret() override;

  // IFWL_Widget
  FWL_Type GetClassID() const override;
  void DrawWidget(CFX_Graphics* pGraphics,
                  const CFX_Matrix* pMatrix = nullptr) override;
  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnDrawWidget(CFX_Graphics* pGraphics,
                    const CFX_Matrix* pMatrix) override;

  void ShowCaret(bool bFlag = true);

 private:
  class Timer : public IFWL_Timer {
   public:
    explicit Timer(IFWL_Caret* pCaret);
    ~Timer() override {}

    void Run(IFWL_TimerInfo* hTimer) override;
  };
  friend class IFWL_Caret::Timer;

  void DrawCaretBK(CFX_Graphics* pGraphics,
                   IFWL_ThemeProvider* pTheme,
                   const CFX_Matrix* pMatrix);

  std::unique_ptr<IFWL_Caret::Timer> m_pTimer;
  IFWL_TimerInfo* m_pTimerInfo;  // not owned.
};

#endif  // XFA_FWL_CORE_IFWL_CARET_H_
