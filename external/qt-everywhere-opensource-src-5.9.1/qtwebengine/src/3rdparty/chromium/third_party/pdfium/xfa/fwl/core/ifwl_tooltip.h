// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_IFWL_TOOLTIP_H_
#define XFA_FWL_CORE_IFWL_TOOLTIP_H_

#include "xfa/fwl/core/ifwl_form.h"
#include "xfa/fwl/core/ifwl_timer.h"

class CFWL_WidgetProperties;
class IFWL_Widget;
class CFWL_ToolTipImpDelegate;

#define FWL_STYLEEXT_TTP_Rectangle (0L << 3)
#define FWL_STYLEEXT_TTP_RoundCorner (1L << 3)
#define FWL_STYLEEXT_TTP_Balloon (1L << 4)
#define FWL_STYLEEXT_TTP_Multiline (1L << 5)
#define FWL_STYLEEXT_TTP_NoAnchor (1L << 6)

class IFWL_ToolTipDP : public IFWL_DataProvider {
 public:
  // IFWL_DataProvider
  void GetCaption(IFWL_Widget* pWidget, CFX_WideString& wsCaption) override = 0;

  virtual int32_t GetInitialDelay(IFWL_Widget* pWidget) = 0;
  virtual int32_t GetAutoPopDelay(IFWL_Widget* pWidget) = 0;
  virtual CFX_DIBitmap* GetToolTipIcon(IFWL_Widget* pWidget) = 0;
  virtual CFX_SizeF GetToolTipIconSize(IFWL_Widget* pWidget) = 0;
};

class IFWL_ToolTip : public IFWL_Form {
 public:
  IFWL_ToolTip(const IFWL_App* app,
               std::unique_ptr<CFWL_WidgetProperties> properties,
               IFWL_Widget* pOuter);
  ~IFWL_ToolTip() override;

  // IFWL_Widget
  FWL_Type GetClassID() const override;
  void GetWidgetRect(CFX_RectF& rect, bool bAutoSize = false) override;
  void Update() override;
  void DrawWidget(CFX_Graphics* pGraphics,
                  const CFX_Matrix* pMatrix = nullptr) override;
  void SetStates(uint32_t dwStates, bool bSet) override;
  void GetClientRect(CFX_RectF& rect) override;
  void OnDrawWidget(CFX_Graphics* pGraphics,
                    const CFX_Matrix* pMatrix) override;

 private:
  class Timer : public IFWL_Timer {
   public:
    explicit Timer(IFWL_ToolTip* pToolTip);
    ~Timer() override {}

    void Run(IFWL_TimerInfo* pTimerInfo) override;
  };
  friend class IFWL_ToolTip::Timer;

  void DrawBkground(CFX_Graphics* pGraphics,
                    IFWL_ThemeProvider* pTheme,
                    const CFX_Matrix* pMatrix);
  void DrawText(CFX_Graphics* pGraphics,
                IFWL_ThemeProvider* pTheme,
                const CFX_Matrix* pMatrix);
  void UpdateTextOutStyles();
  void RefreshToolTipPos();

  CFX_RectF m_rtClient;
  CFX_RectF m_rtCaption;
  uint32_t m_dwTTOStyles;
  int32_t m_iTTOAlign;
  CFX_RectF m_rtAnchor;
  IFWL_TimerInfo* m_pTimerInfoShow;
  IFWL_TimerInfo* m_pTimerInfoHide;
  IFWL_ToolTip::Timer m_TimerShow;
  IFWL_ToolTip::Timer m_TimerHide;
};

#endif  // XFA_FWL_CORE_IFWL_TOOLTIP_H_
