// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_IFWL_SCROLLBAR_H_
#define XFA_FWL_CORE_IFWL_SCROLLBAR_H_

#include "core/fxcrt/fx_system.h"
#include "xfa/fwl/core/cfwl_evtscroll.h"
#include "xfa/fwl/core/cfwl_widgetproperties.h"
#include "xfa/fwl/core/ifwl_dataprovider.h"
#include "xfa/fwl/core/ifwl_timer.h"
#include "xfa/fwl/core/ifwl_widget.h"

class IFWL_Widget;

#define FWL_STYLEEXT_SCB_Horz (0L << 0)
#define FWL_STYLEEXT_SCB_Vert (1L << 0)

class IFWL_ScrollBarDP : public IFWL_DataProvider {};

class IFWL_ScrollBar : public IFWL_Widget {
 public:
  IFWL_ScrollBar(const IFWL_App* app,
                 std::unique_ptr<CFWL_WidgetProperties> properties,
                 IFWL_Widget* pOuter);
  ~IFWL_ScrollBar() override;

  // IFWL_Widget
  FWL_Type GetClassID() const override;
  void GetWidgetRect(CFX_RectF& rect, bool bAutoSize = false) override;
  void Update() override;
  void DrawWidget(CFX_Graphics* pGraphics,
                  const CFX_Matrix* pMatrix = nullptr) override;
  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnDrawWidget(CFX_Graphics* pGraphics,
                    const CFX_Matrix* pMatrix) override;

  void GetRange(FX_FLOAT* fMin, FX_FLOAT* fMax) const {
    ASSERT(fMin);
    ASSERT(fMax);
    *fMin = m_fRangeMin;
    *fMax = m_fRangeMax;
  }
  void SetRange(FX_FLOAT fMin, FX_FLOAT fMax) {
    m_fRangeMin = fMin;
    m_fRangeMax = fMax;
  }
  FX_FLOAT GetPageSize() const { return m_fPageSize; }
  void SetPageSize(FX_FLOAT fPageSize) { m_fPageSize = fPageSize; }
  FX_FLOAT GetStepSize() const { return m_fStepSize; }
  void SetStepSize(FX_FLOAT fStepSize) { m_fStepSize = fStepSize; }
  FX_FLOAT GetPos() const { return m_fPos; }
  void SetPos(FX_FLOAT fPos) { m_fPos = fPos; }
  void SetTrackPos(FX_FLOAT fTrackPos);

 private:
  class Timer : public IFWL_Timer {
   public:
    explicit Timer(IFWL_ScrollBar* pToolTip);
    ~Timer() override {}

    void Run(IFWL_TimerInfo* pTimerInfo) override;
  };
  friend class IFWL_ScrollBar::Timer;

  bool IsVertical() const {
    return !!(m_pProperties->m_dwStyleExes & FWL_STYLEEXT_SCB_Vert);
  }
  void DrawTrack(CFX_Graphics* pGraphics,
                 IFWL_ThemeProvider* pTheme,
                 bool bLower = true,
                 const CFX_Matrix* pMatrix = nullptr);
  void DrawArrowBtn(CFX_Graphics* pGraphics,
                    IFWL_ThemeProvider* pTheme,
                    bool bMinBtn = true,
                    const CFX_Matrix* pMatrix = nullptr);
  void DrawThumb(CFX_Graphics* pGraphics,
                 IFWL_ThemeProvider* pTheme,
                 const CFX_Matrix* pMatrix = nullptr);
  void Layout();
  void CalcButtonLen();
  void CalcMinButtonRect(CFX_RectF& rect);
  void CalcMaxButtonRect(CFX_RectF& rect);
  void CalcThumbButtonRect(CFX_RectF& rect);
  void CalcMinTrackRect(CFX_RectF& rect);
  void CalcMaxTrackRect(CFX_RectF& rect);
  FX_FLOAT GetTrackPointPos(FX_FLOAT fx, FX_FLOAT fy);
  void GetTrackRect(CFX_RectF& rect, bool bLower = true);
  bool SendEvent();
  bool OnScroll(FWL_SCBCODE dwCode, FX_FLOAT fPos);
  void OnLButtonDown(uint32_t dwFlags, FX_FLOAT fx, FX_FLOAT fy);
  void OnLButtonUp(uint32_t dwFlags, FX_FLOAT fx, FX_FLOAT fy);
  void OnMouseMove(uint32_t dwFlags, FX_FLOAT fx, FX_FLOAT fy);
  void OnMouseLeave();
  void OnMouseWheel(FX_FLOAT fx,
                    FX_FLOAT fy,
                    uint32_t dwFlags,
                    FX_FLOAT fDeltaX,
                    FX_FLOAT fDeltaY);
  bool DoScroll(FWL_SCBCODE dwCode, FX_FLOAT fPos = 0.0f);
  void DoMouseDown(int32_t iItem,
                   const CFX_RectF& rtItem,
                   int32_t& iState,
                   FX_FLOAT fx,
                   FX_FLOAT fy);
  void DoMouseUp(int32_t iItem,
                 const CFX_RectF& rtItem,
                 int32_t& iState,
                 FX_FLOAT fx,
                 FX_FLOAT fy);
  void DoMouseMove(int32_t iItem,
                   const CFX_RectF& rtItem,
                   int32_t& iState,
                   FX_FLOAT fx,
                   FX_FLOAT fy);
  void DoMouseLeave(int32_t iItem, const CFX_RectF& rtItem, int32_t& iState);
  void DoMouseHover(int32_t iItem, const CFX_RectF& rtItem, int32_t& iState);

  IFWL_TimerInfo* m_pTimerInfo;
  FX_FLOAT m_fRangeMin;
  FX_FLOAT m_fRangeMax;
  FX_FLOAT m_fPageSize;
  FX_FLOAT m_fStepSize;
  FX_FLOAT m_fPos;
  FX_FLOAT m_fTrackPos;
  int32_t m_iMinButtonState;
  int32_t m_iMaxButtonState;
  int32_t m_iThumbButtonState;
  int32_t m_iMinTrackState;
  int32_t m_iMaxTrackState;
  FX_FLOAT m_fLastTrackPos;
  FX_FLOAT m_cpTrackPointX;
  FX_FLOAT m_cpTrackPointY;
  int32_t m_iMouseWheel;
  bool m_bMouseDown;
  FX_FLOAT m_fButtonLen;
  bool m_bMinSize;
  CFX_RectF m_rtClient;
  CFX_RectF m_rtThumb;
  CFX_RectF m_rtMinBtn;
  CFX_RectF m_rtMaxBtn;
  CFX_RectF m_rtMinTrack;
  CFX_RectF m_rtMaxTrack;
  bool m_bCustomLayout;
  FX_FLOAT m_fMinThumb;
  IFWL_ScrollBar::Timer m_Timer;
};

#endif  // XFA_FWL_CORE_IFWL_SCROLLBAR_H_
