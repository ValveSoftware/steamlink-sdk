// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/ifwl_tooltip.h"

#include "third_party/base/ptr_util.h"
#include "xfa/fde/tto/fde_textout.h"
#include "xfa/fwl/core/cfwl_themebackground.h"
#include "xfa/fwl/core/cfwl_themepart.h"
#include "xfa/fwl/core/cfwl_themetext.h"
#include "xfa/fwl/core/fwl_noteimp.h"
#include "xfa/fwl/core/ifwl_themeprovider.h"
#include "xfa/fwl/core/ifwl_tooltip.h"
#include "xfa/fwl/theme/cfwl_widgettp.h"

IFWL_ToolTip::IFWL_ToolTip(const IFWL_App* app,
                           std::unique_ptr<CFWL_WidgetProperties> properties,
                           IFWL_Widget* pOuter)
    : IFWL_Form(app, std::move(properties), pOuter),
      m_dwTTOStyles(FDE_TTOSTYLE_SingleLine),
      m_iTTOAlign(FDE_TTOALIGNMENT_Center),
      m_pTimerInfoShow(nullptr),
      m_pTimerInfoHide(nullptr),
      m_TimerShow(this),
      m_TimerHide(this) {
  m_rtClient.Set(0, 0, 0, 0);
  m_rtCaption.Set(0, 0, 0, 0);
  m_rtAnchor.Set(0, 0, 0, 0);
  m_pProperties->m_dwStyles &= ~FWL_WGTSTYLE_Child;
  m_pProperties->m_dwStyles |= FWL_WGTSTYLE_Popup;
}

IFWL_ToolTip::~IFWL_ToolTip() {}

FWL_Type IFWL_ToolTip::GetClassID() const {
  return FWL_Type::ToolTip;
}

void IFWL_ToolTip::GetWidgetRect(CFX_RectF& rect, bool bAutoSize) {
  if (!bAutoSize) {
    rect = m_pProperties->m_rtWidget;
    return;
  }

  rect.Set(0, 0, 0, 0);
  if (!m_pProperties->m_pThemeProvider)
    m_pProperties->m_pThemeProvider = GetAvailableTheme();

  CFX_WideString wsCaption;
  IFWL_ToolTipDP* pData =
      static_cast<IFWL_ToolTipDP*>(m_pProperties->m_pDataProvider);
  if (pData)
    pData->GetCaption(this, wsCaption);

  int32_t iLen = wsCaption.GetLength();
  if (iLen > 0) {
    CFX_SizeF sz = CalcTextSize(wsCaption, m_pProperties->m_pThemeProvider);
    rect.Set(0, 0, sz.x, sz.y);
    rect.width += 25;
    rect.height += 16;
  }
  IFWL_Widget::GetWidgetRect(rect, true);
}

void IFWL_ToolTip::Update() {
  if (IsLocked())
    return;
  if (!m_pProperties->m_pThemeProvider)
    m_pProperties->m_pThemeProvider = GetAvailableTheme();

  UpdateTextOutStyles();
  GetClientRect(m_rtClient);
  m_rtCaption = m_rtClient;
}

void IFWL_ToolTip::GetClientRect(CFX_RectF& rect) {
  FX_FLOAT x = 0;
  FX_FLOAT y = 0;
  FX_FLOAT t = 0;
  IFWL_ThemeProvider* pTheme = m_pProperties->m_pThemeProvider;
  if (pTheme) {
    CFWL_ThemePart part;
    part.m_pWidget = this;
    x = *static_cast<FX_FLOAT*>(
        pTheme->GetCapacity(&part, CFWL_WidgetCapacity::CXBorder));
    y = *static_cast<FX_FLOAT*>(
        pTheme->GetCapacity(&part, CFWL_WidgetCapacity::CYBorder));
  }
  rect = m_pProperties->m_rtWidget;
  rect.Offset(-rect.left, -rect.top);
  rect.Deflate(x, t, x, y);
}

void IFWL_ToolTip::DrawWidget(CFX_Graphics* pGraphics,
                              const CFX_Matrix* pMatrix) {
  if (!pGraphics || !m_pProperties->m_pThemeProvider)
    return;

  IFWL_ThemeProvider* pTheme = m_pProperties->m_pThemeProvider;
  DrawBkground(pGraphics, pTheme, pMatrix);
  DrawText(pGraphics, pTheme, pMatrix);
}

void IFWL_ToolTip::DrawBkground(CFX_Graphics* pGraphics,
                                IFWL_ThemeProvider* pTheme,
                                const CFX_Matrix* pMatrix) {
  CFWL_ThemeBackground param;
  param.m_pWidget = this;
  param.m_iPart = CFWL_Part::Background;
  param.m_dwStates = m_pProperties->m_dwStates;
  param.m_pGraphics = pGraphics;
  if (pMatrix)
    param.m_matrix.Concat(*pMatrix);
  param.m_rtPart = m_rtClient;
  if (m_pProperties->m_dwStates & FWL_WGTSTATE_Focused)
    param.m_pData = &m_rtCaption;
  pTheme->DrawBackground(&param);
}

void IFWL_ToolTip::DrawText(CFX_Graphics* pGraphics,
                            IFWL_ThemeProvider* pTheme,
                            const CFX_Matrix* pMatrix) {
  if (!m_pProperties->m_pDataProvider)
    return;

  CFX_WideString wsCaption;
  m_pProperties->m_pDataProvider->GetCaption(this, wsCaption);
  if (wsCaption.IsEmpty())
    return;

  CFWL_ThemeText param;
  param.m_pWidget = this;
  param.m_iPart = CFWL_Part::Caption;
  param.m_dwStates = m_pProperties->m_dwStates;
  param.m_pGraphics = pGraphics;
  if (pMatrix)
    param.m_matrix.Concat(*pMatrix);
  param.m_rtPart = m_rtCaption;
  param.m_wsText = wsCaption;
  param.m_dwTTOStyles = m_dwTTOStyles;
  param.m_iTTOAlign = m_iTTOAlign;
  pTheme->DrawText(&param);
}

void IFWL_ToolTip::UpdateTextOutStyles() {
  m_iTTOAlign = FDE_TTOALIGNMENT_Center;
  m_dwTTOStyles = FDE_TTOSTYLE_SingleLine;
  if (m_pProperties->m_dwStyleExes & FWL_WGTSTYLE_RTLReading)
    m_dwTTOStyles |= FDE_TTOSTYLE_RTL;
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_TTP_Multiline)
    m_dwTTOStyles &= ~FDE_TTOSTYLE_SingleLine;
}

void IFWL_ToolTip::SetStates(uint32_t dwStates, bool bSet) {
  if ((dwStates & FWL_WGTSTATE_Invisible) && !bSet) {
    IFWL_ToolTipDP* pData =
        static_cast<IFWL_ToolTipDP*>(m_pProperties->m_pDataProvider);
    int32_t nAutoPopDelay = pData->GetAutoPopDelay(this);
    m_pTimerInfoHide = m_TimerHide.StartTimer(nAutoPopDelay, false);
  }
  IFWL_Widget::SetStates(dwStates, bSet);
}

void IFWL_ToolTip::RefreshToolTipPos() {
  if ((m_pProperties->m_dwStyleExes & FWL_STYLEEXT_TTP_NoAnchor) == 0) {
    CFX_RectF rtPopup;
    CFX_RectF rtWidget(m_pProperties->m_rtWidget);
    CFX_RectF rtAnchor(m_rtAnchor);
    rtPopup.Set(0, 0, 0, 0);
    FX_FLOAT fx = rtAnchor.Center().x + 20;
    FX_FLOAT fy = rtAnchor.Center().y + 20;
    rtPopup.Set(fx, fy, rtWidget.Width(), rtWidget.Height());

    if (rtPopup.bottom() > 0.0f)
      rtPopup.Offset(0, 0.0f - rtPopup.bottom());
    if (rtPopup.right() > 0.0f)
      rtPopup.Offset(0.0f - rtPopup.right(), 0);
    if (rtPopup.left < 0)
      rtPopup.Offset(0 - rtPopup.left, 0);
    if (rtPopup.top < 0)
      rtPopup.Offset(0, 0 - rtPopup.top);

    SetWidgetRect(rtPopup);
    Update();
  }
}

void IFWL_ToolTip::OnDrawWidget(CFX_Graphics* pGraphics,
                                const CFX_Matrix* pMatrix) {
  DrawWidget(pGraphics, pMatrix);
}

IFWL_ToolTip::Timer::Timer(IFWL_ToolTip* pToolTip) : IFWL_Timer(pToolTip) {}

void IFWL_ToolTip::Timer::Run(IFWL_TimerInfo* pTimerInfo) {
  IFWL_ToolTip* pToolTip = static_cast<IFWL_ToolTip*>(m_pWidget);

  if (pToolTip->m_pTimerInfoShow == pTimerInfo && pToolTip->m_pTimerInfoShow) {
    if (pToolTip->GetStates() & FWL_WGTSTATE_Invisible) {
      pToolTip->SetStates(FWL_WGTSTATE_Invisible, false);
      pToolTip->RefreshToolTipPos();
      pToolTip->m_pTimerInfoShow->StopTimer();
      pToolTip->m_pTimerInfoShow = nullptr;
      return;
    }
  }
  if (pToolTip->m_pTimerInfoHide == pTimerInfo && pToolTip->m_pTimerInfoHide) {
    pToolTip->SetStates(FWL_WGTSTATE_Invisible, true);
    pToolTip->m_pTimerInfoHide->StopTimer();
    pToolTip->m_pTimerInfoHide = nullptr;
  }
}
