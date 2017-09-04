// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/ifwl_caret.h"

#include "third_party/base/ptr_util.h"
#include "xfa/fwl/core/cfwl_themebackground.h"
#include "xfa/fwl/core/cfwl_widgetproperties.h"
#include "xfa/fwl/core/fwl_noteimp.h"
#include "xfa/fwl/core/ifwl_caret.h"
#include "xfa/fwl/core/ifwl_themeprovider.h"

namespace {

const uint32_t kFrequency = 400;

}  // namespace

IFWL_Caret::IFWL_Caret(const IFWL_App* app,
                       std::unique_ptr<CFWL_WidgetProperties> properties,
                       IFWL_Widget* pOuter)
    : IFWL_Widget(app, std::move(properties), pOuter),
      m_pTimer(new IFWL_Caret::Timer(this)),
      m_pTimerInfo(nullptr) {
  SetStates(FWL_STATE_CAT_HightLight);
}

IFWL_Caret::~IFWL_Caret() {
  if (m_pTimerInfo) {
    m_pTimerInfo->StopTimer();
    m_pTimerInfo = nullptr;
  }
}

FWL_Type IFWL_Caret::GetClassID() const {
  return FWL_Type::Caret;
}

void IFWL_Caret::DrawWidget(CFX_Graphics* pGraphics,
                            const CFX_Matrix* pMatrix) {
  if (!pGraphics)
    return;
  if (!m_pProperties->m_pThemeProvider)
    m_pProperties->m_pThemeProvider = GetAvailableTheme();
  if (!m_pProperties->m_pThemeProvider)
    return;

  DrawCaretBK(pGraphics, m_pProperties->m_pThemeProvider, pMatrix);
}

void IFWL_Caret::ShowCaret(bool bFlag) {
  if (m_pTimerInfo) {
    m_pTimerInfo->StopTimer();
    m_pTimerInfo = nullptr;
  }
  if (bFlag)
    m_pTimerInfo = m_pTimer->StartTimer(kFrequency, true);

  SetStates(FWL_WGTSTATE_Invisible, !bFlag);
}

void IFWL_Caret::DrawCaretBK(CFX_Graphics* pGraphics,
                             IFWL_ThemeProvider* pTheme,
                             const CFX_Matrix* pMatrix) {
  if (!(m_pProperties->m_dwStates & FWL_STATE_CAT_HightLight))
    return;

  CFX_RectF rect;
  GetWidgetRect(rect);
  rect.Set(0, 0, rect.width, rect.height);

  CFWL_ThemeBackground param;
  param.m_pWidget = this;
  param.m_pGraphics = pGraphics;
  param.m_rtPart = rect;
  param.m_iPart = CFWL_Part::Background;
  param.m_dwStates = CFWL_PartState_HightLight;
  if (pMatrix)
    param.m_matrix.Concat(*pMatrix);
  pTheme->DrawBackground(&param);
}

void IFWL_Caret::OnProcessMessage(CFWL_Message* pMessage) {}

void IFWL_Caret::OnDrawWidget(CFX_Graphics* pGraphics,
                              const CFX_Matrix* pMatrix) {
  DrawWidget(pGraphics, pMatrix);
}

IFWL_Caret::Timer::Timer(IFWL_Caret* pCaret) : IFWL_Timer(pCaret) {}

void IFWL_Caret::Timer::Run(IFWL_TimerInfo* pTimerInfo) {
  IFWL_Caret* pCaret = static_cast<IFWL_Caret*>(m_pWidget);
  pCaret->SetStates(FWL_STATE_CAT_HightLight,
                    !(pCaret->GetStates() & FWL_STATE_CAT_HightLight));

  CFX_RectF rt;
  pCaret->GetWidgetRect(rt);
  rt.Set(0, 0, rt.width + 1, rt.height);
  pCaret->Repaint(&rt);
}
