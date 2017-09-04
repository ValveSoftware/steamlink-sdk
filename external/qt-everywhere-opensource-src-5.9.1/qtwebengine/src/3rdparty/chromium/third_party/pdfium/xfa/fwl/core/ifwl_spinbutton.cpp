// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/ifwl_spinbutton.h"

#include "third_party/base/ptr_util.h"
#include "xfa/fwl/core/cfwl_evtclick.h"
#include "xfa/fwl/core/cfwl_msgkey.h"
#include "xfa/fwl/core/cfwl_msgmouse.h"
#include "xfa/fwl/core/cfwl_themebackground.h"
#include "xfa/fwl/core/cfwl_widgetproperties.h"
#include "xfa/fwl/core/fwl_noteimp.h"
#include "xfa/fwl/core/ifwl_spinbutton.h"
#include "xfa/fwl/core/ifwl_themeprovider.h"
#include "xfa/fwl/core/ifwl_themeprovider.h"
#include "xfa/fwl/core/ifwl_timer.h"

namespace {

const int kMinWidth = 18;
const int kMinHeight = 32;
const int kElapseTime = 200;

}  // namespace

IFWL_SpinButton::IFWL_SpinButton(
    const IFWL_App* app,
    std::unique_ptr<CFWL_WidgetProperties> properties)
    : IFWL_Widget(app, std::move(properties), nullptr),
      m_dwUpState(CFWL_PartState_Normal),
      m_dwDnState(CFWL_PartState_Normal),
      m_iButtonIndex(0),
      m_bLButtonDwn(false),
      m_pTimerInfo(nullptr),
      m_Timer(this) {
  m_rtClient.Reset();
  m_rtUpButton.Reset();
  m_rtDnButton.Reset();
  m_pProperties->m_dwStyleExes |= FWL_STYLEEXE_SPB_Vert;
}

IFWL_SpinButton::~IFWL_SpinButton() {}

FWL_Type IFWL_SpinButton::GetClassID() const {
  return FWL_Type::SpinButton;
}

void IFWL_SpinButton::GetWidgetRect(CFX_RectF& rect, bool bAutoSize) {
  if (!bAutoSize) {
    rect = m_pProperties->m_rtWidget;
    return;
  }

  rect.Set(0, 0, kMinWidth, kMinHeight);
  IFWL_Widget::GetWidgetRect(rect, true);
}

void IFWL_SpinButton::Update() {
  if (IsLocked())
    return;

  GetClientRect(m_rtClient);
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXE_SPB_Vert) {
    m_rtUpButton.Set(m_rtClient.top, m_rtClient.left, m_rtClient.width,
                     m_rtClient.height / 2);
    m_rtDnButton.Set(m_rtClient.left, m_rtClient.top + m_rtClient.height / 2,
                     m_rtClient.width, m_rtClient.height / 2);
  } else {
    m_rtUpButton.Set(m_rtClient.left, m_rtClient.top, m_rtClient.width / 2,
                     m_rtClient.height);
    m_rtDnButton.Set(m_rtClient.left + m_rtClient.width / 2, m_rtClient.top,
                     m_rtClient.width / 2, m_rtClient.height);
  }
}

FWL_WidgetHit IFWL_SpinButton::HitTest(FX_FLOAT fx, FX_FLOAT fy) {
  if (m_rtClient.Contains(fx, fy))
    return FWL_WidgetHit::Client;
  if (HasBorder() && (m_rtClient.Contains(fx, fy)))
    return FWL_WidgetHit::Border;
  if (HasEdge()) {
    CFX_RectF rtEdge;
    GetEdgeRect(rtEdge);
    if (rtEdge.Contains(fx, fy))
      return FWL_WidgetHit::Left;
  }
  if (m_rtUpButton.Contains(fx, fy))
    return FWL_WidgetHit::UpButton;
  if (m_rtDnButton.Contains(fx, fy))
    return FWL_WidgetHit::DownButton;
  return FWL_WidgetHit::Unknown;
}

void IFWL_SpinButton::DrawWidget(CFX_Graphics* pGraphics,
                                 const CFX_Matrix* pMatrix) {
  if (!pGraphics)
    return;

  CFX_RectF rtClip(m_rtClient);
  if (pMatrix)
    pMatrix->TransformRect(rtClip);

  IFWL_ThemeProvider* pTheme = GetAvailableTheme();
  if (HasBorder())
    DrawBorder(pGraphics, CFWL_Part::Border, pTheme, pMatrix);
  if (HasEdge())
    DrawEdge(pGraphics, CFWL_Part::Edge, pTheme, pMatrix);

  DrawUpButton(pGraphics, pTheme, pMatrix);
  DrawDownButton(pGraphics, pTheme, pMatrix);
}

void IFWL_SpinButton::EnableButton(bool bEnable, bool bUp) {
  if (bUp)
    m_dwUpState = bEnable ? CFWL_PartState_Normal : CFWL_PartState_Disabled;
  else
    m_dwDnState = bEnable ? CFWL_PartState_Normal : CFWL_PartState_Disabled;
}

bool IFWL_SpinButton::IsButtonEnabled(bool bUp) {
  if (bUp)
    return (m_dwUpState != CFWL_PartState_Disabled);
  return (m_dwDnState != CFWL_PartState_Disabled);
}

void IFWL_SpinButton::DrawUpButton(CFX_Graphics* pGraphics,
                                   IFWL_ThemeProvider* pTheme,
                                   const CFX_Matrix* pMatrix) {
  CFWL_ThemeBackground params;
  params.m_pWidget = this;
  params.m_iPart = CFWL_Part::UpButton;
  params.m_pGraphics = pGraphics;
  params.m_dwStates = m_dwUpState + 1;
  if (pMatrix)
    params.m_matrix.Concat(*pMatrix);

  params.m_rtPart = m_rtUpButton;
  pTheme->DrawBackground(&params);
}

void IFWL_SpinButton::DrawDownButton(CFX_Graphics* pGraphics,
                                     IFWL_ThemeProvider* pTheme,
                                     const CFX_Matrix* pMatrix) {
  CFWL_ThemeBackground params;
  params.m_pWidget = this;
  params.m_iPart = CFWL_Part::DownButton;
  params.m_pGraphics = pGraphics;
  params.m_dwStates = m_dwDnState + 1;
  if (pMatrix)
    params.m_matrix.Concat(*pMatrix);

  params.m_rtPart = m_rtDnButton;
  pTheme->DrawBackground(&params);
}

void IFWL_SpinButton::OnProcessMessage(CFWL_Message* pMessage) {
  if (!pMessage)
    return;

  CFWL_MessageType dwMsgCode = pMessage->GetClassID();
  switch (dwMsgCode) {
    case CFWL_MessageType::SetFocus: {
      OnFocusChanged(pMessage, true);
      break;
    }
    case CFWL_MessageType::KillFocus: {
      OnFocusChanged(pMessage, false);
      break;
    }
    case CFWL_MessageType::Mouse: {
      CFWL_MsgMouse* pMsg = static_cast<CFWL_MsgMouse*>(pMessage);
      switch (pMsg->m_dwCmd) {
        case FWL_MouseCommand::LeftButtonDown:
          OnLButtonDown(pMsg);
          break;
        case FWL_MouseCommand::LeftButtonUp:
          OnLButtonUp(pMsg);
          break;
        case FWL_MouseCommand::Move:
          OnMouseMove(pMsg);
          break;
        case FWL_MouseCommand::Leave:
          OnMouseLeave(pMsg);
          break;
        default:
          break;
      }
      break;
    }
    case CFWL_MessageType::Key: {
      CFWL_MsgKey* pKey = static_cast<CFWL_MsgKey*>(pMessage);
      if (pKey->m_dwCmd == FWL_KeyCommand::KeyDown)
        OnKeyDown(pKey);
      break;
    }
    default:
      break;
  }
  IFWL_Widget::OnProcessMessage(pMessage);
}

void IFWL_SpinButton::OnDrawWidget(CFX_Graphics* pGraphics,
                                   const CFX_Matrix* pMatrix) {
  DrawWidget(pGraphics, pMatrix);
}

void IFWL_SpinButton::OnFocusChanged(CFWL_Message* pMsg, bool bSet) {
  if (bSet)
    m_pProperties->m_dwStates |= (FWL_WGTSTATE_Focused);
  else
    m_pProperties->m_dwStates &= ~(FWL_WGTSTATE_Focused);

  Repaint(&m_rtClient);
}

void IFWL_SpinButton::OnLButtonDown(CFWL_MsgMouse* pMsg) {
  m_bLButtonDwn = true;
  SetGrab(true);
  SetFocus(true);
  if (!m_pProperties->m_pDataProvider)
    return;

  bool bUpPress =
      (m_rtUpButton.Contains(pMsg->m_fx, pMsg->m_fy) && IsButtonEnabled(true));
  bool bDnPress =
      (m_rtDnButton.Contains(pMsg->m_fx, pMsg->m_fy) && IsButtonEnabled(false));
  if (!bUpPress && !bDnPress)
    return;
  if (bUpPress) {
    m_iButtonIndex = 0;
    m_dwUpState = CFWL_PartState_Pressed;
  }
  if (bDnPress) {
    m_iButtonIndex = 1;
    m_dwDnState = CFWL_PartState_Pressed;
  }

  CFWL_EvtClick wmPosChanged;
  wmPosChanged.m_pSrcTarget = this;
  DispatchEvent(&wmPosChanged);

  Repaint(bUpPress ? &m_rtUpButton : &m_rtDnButton);
  m_pTimerInfo = m_Timer.StartTimer(kElapseTime, true);
}

void IFWL_SpinButton::OnLButtonUp(CFWL_MsgMouse* pMsg) {
  if (m_pProperties->m_dwStates & CFWL_PartState_Disabled)
    return;

  m_bLButtonDwn = false;
  SetGrab(false);
  SetFocus(false);
  if (m_pTimerInfo) {
    m_pTimerInfo->StopTimer();
    m_pTimerInfo = nullptr;
  }
  bool bRepaint = false;
  CFX_RectF rtInvalidate;
  if (m_dwUpState == CFWL_PartState_Pressed && IsButtonEnabled(true)) {
    m_dwUpState = CFWL_PartState_Normal;
    bRepaint = true;
    rtInvalidate = m_rtUpButton;
  } else if (m_dwDnState == CFWL_PartState_Pressed && IsButtonEnabled(false)) {
    m_dwDnState = CFWL_PartState_Normal;
    bRepaint = true;
    rtInvalidate = m_rtDnButton;
  }
  if (bRepaint)
    Repaint(&rtInvalidate);
}

void IFWL_SpinButton::OnMouseMove(CFWL_MsgMouse* pMsg) {
  if (!m_pProperties->m_pDataProvider)
    return;
  if (m_bLButtonDwn)
    return;

  bool bRepaint = false;
  CFX_RectF rtInvlidate;
  rtInvlidate.Reset();
  if (m_rtUpButton.Contains(pMsg->m_fx, pMsg->m_fy)) {
    if (IsButtonEnabled(true)) {
      if (m_dwUpState == CFWL_PartState_Hovered) {
        m_dwUpState = CFWL_PartState_Hovered;
        bRepaint = true;
        rtInvlidate = m_rtUpButton;
      }
      if (m_dwDnState != CFWL_PartState_Normal && IsButtonEnabled(false)) {
        m_dwDnState = CFWL_PartState_Normal;
        if (bRepaint)
          rtInvlidate.Union(m_rtDnButton);
        else
          rtInvlidate = m_rtDnButton;

        bRepaint = true;
      }
    }
    if (!IsButtonEnabled(false))
      EnableButton(false, false);

  } else if (m_rtDnButton.Contains(pMsg->m_fx, pMsg->m_fy)) {
    if (IsButtonEnabled(false)) {
      if (m_dwDnState != CFWL_PartState_Hovered) {
        m_dwDnState = CFWL_PartState_Hovered;
        bRepaint = true;
        rtInvlidate = m_rtDnButton;
      }
      if (m_dwUpState != CFWL_PartState_Normal && IsButtonEnabled(true)) {
        m_dwUpState = CFWL_PartState_Normal;
        if (bRepaint)
          rtInvlidate.Union(m_rtUpButton);
        else
          rtInvlidate = m_rtUpButton;
        bRepaint = true;
      }
    }
  } else if (m_dwUpState != CFWL_PartState_Normal ||
             m_dwDnState != CFWL_PartState_Normal) {
    if (m_dwUpState != CFWL_PartState_Normal) {
      m_dwUpState = CFWL_PartState_Normal;
      bRepaint = true;
      rtInvlidate = m_rtUpButton;
    }
    if (m_dwDnState != CFWL_PartState_Normal) {
      m_dwDnState = CFWL_PartState_Normal;
      if (bRepaint)
        rtInvlidate.Union(m_rtDnButton);
      else
        rtInvlidate = m_rtDnButton;

      bRepaint = true;
    }
  }
  if (bRepaint)
    Repaint(&rtInvlidate);
}

void IFWL_SpinButton::OnMouseLeave(CFWL_MsgMouse* pMsg) {
  if (!pMsg)
    return;
  if (m_dwUpState != CFWL_PartState_Normal && IsButtonEnabled(true))
    m_dwUpState = CFWL_PartState_Normal;
  if (m_dwDnState != CFWL_PartState_Normal && IsButtonEnabled(false))
    m_dwDnState = CFWL_PartState_Normal;

  Repaint(&m_rtClient);
}

void IFWL_SpinButton::OnKeyDown(CFWL_MsgKey* pMsg) {
  if (!m_pProperties->m_pDataProvider)
    return;

  bool bUp =
      pMsg->m_dwKeyCode == FWL_VKEY_Up || pMsg->m_dwKeyCode == FWL_VKEY_Left;
  bool bDown =
      pMsg->m_dwKeyCode == FWL_VKEY_Down || pMsg->m_dwKeyCode == FWL_VKEY_Right;
  if (!bUp && !bDown)
    return;

  bool bUpEnable = IsButtonEnabled(true);
  bool bDownEnable = IsButtonEnabled(false);
  if (!bUpEnable && !bDownEnable)
    return;

  CFWL_EvtClick wmPosChanged;
  wmPosChanged.m_pSrcTarget = this;
  DispatchEvent(&wmPosChanged);

  Repaint(bUpEnable ? &m_rtUpButton : &m_rtDnButton);
}

IFWL_SpinButton::Timer::Timer(IFWL_SpinButton* pToolTip)
    : IFWL_Timer(pToolTip) {}

void IFWL_SpinButton::Timer::Run(IFWL_TimerInfo* pTimerInfo) {
  IFWL_SpinButton* pButton = static_cast<IFWL_SpinButton*>(m_pWidget);

  if (!pButton->m_pTimerInfo)
    return;

  CFWL_EvtClick wmPosChanged;
  wmPosChanged.m_pSrcTarget = pButton;
  pButton->DispatchEvent(&wmPosChanged);
}
