// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/ifwl_pushbutton.h"

#include "third_party/base/ptr_util.h"
#include "xfa/fde/tto/fde_textout.h"
#include "xfa/fwl/core/cfwl_evtclick.h"
#include "xfa/fwl/core/cfwl_evtmouse.h"
#include "xfa/fwl/core/cfwl_msgkey.h"
#include "xfa/fwl/core/cfwl_msgmouse.h"
#include "xfa/fwl/core/cfwl_themebackground.h"
#include "xfa/fwl/core/cfwl_themetext.h"
#include "xfa/fwl/core/fwl_noteimp.h"
#include "xfa/fwl/core/ifwl_pushbutton.h"
#include "xfa/fwl/core/ifwl_themeprovider.h"

IFWL_PushButton::IFWL_PushButton(
    const IFWL_App* app,
    std::unique_ptr<CFWL_WidgetProperties> properties)
    : IFWL_Widget(app, std::move(properties), nullptr),
      m_bBtnDown(false),
      m_dwTTOStyles(FDE_TTOSTYLE_SingleLine),
      m_iTTOAlign(FDE_TTOALIGNMENT_Center) {
  m_rtClient.Set(0, 0, 0, 0);
  m_rtCaption.Set(0, 0, 0, 0);
}

IFWL_PushButton::~IFWL_PushButton() {}

FWL_Type IFWL_PushButton::GetClassID() const {
  return FWL_Type::PushButton;
}

void IFWL_PushButton::GetWidgetRect(CFX_RectF& rect, bool bAutoSize) {
  if (!bAutoSize) {
    rect = m_pProperties->m_rtWidget;
    return;
  }

  rect.Set(0, 0, 0, 0);
  if (!m_pProperties->m_pThemeProvider)
    m_pProperties->m_pThemeProvider = GetAvailableTheme();

  CFX_WideString wsCaption;
  IFWL_PushButtonDP* pData =
      static_cast<IFWL_PushButtonDP*>(m_pProperties->m_pDataProvider);
  if (pData)
    pData->GetCaption(this, wsCaption);

  int32_t iLen = wsCaption.GetLength();
  if (iLen > 0) {
    CFX_SizeF sz = CalcTextSize(wsCaption, m_pProperties->m_pThemeProvider);
    rect.Set(0, 0, sz.x, sz.y);
  }

  FX_FLOAT* fcaption =
      static_cast<FX_FLOAT*>(GetThemeCapacity(CFWL_WidgetCapacity::Margin));
  rect.Inflate(*fcaption, *fcaption);
  IFWL_Widget::GetWidgetRect(rect, true);
}

void IFWL_PushButton::SetStates(uint32_t dwStates, bool bSet) {
  if ((dwStates & FWL_WGTSTATE_Disabled) && bSet) {
    m_pProperties->m_dwStates = FWL_WGTSTATE_Disabled;
    return;
  }
  IFWL_Widget::SetStates(dwStates, bSet);
}

void IFWL_PushButton::Update() {
  if (IsLocked())
    return;
  if (!m_pProperties->m_pThemeProvider)
    m_pProperties->m_pThemeProvider = GetAvailableTheme();

  UpdateTextOutStyles();
  GetClientRect(m_rtClient);
  m_rtCaption = m_rtClient;
  FX_FLOAT* fcaption =
      static_cast<FX_FLOAT*>(GetThemeCapacity(CFWL_WidgetCapacity::Margin));
  m_rtCaption.Inflate(-*fcaption, -*fcaption);
}

void IFWL_PushButton::DrawWidget(CFX_Graphics* pGraphics,
                                 const CFX_Matrix* pMatrix) {
  if (!pGraphics)
    return;
  if (!m_pProperties->m_pThemeProvider)
    return;

  IFWL_PushButtonDP* pData =
      static_cast<IFWL_PushButtonDP*>(m_pProperties->m_pDataProvider);
  IFWL_ThemeProvider* pTheme = m_pProperties->m_pThemeProvider;
  if (HasBorder()) {
    DrawBorder(pGraphics, CFWL_Part::Border, m_pProperties->m_pThemeProvider,
               pMatrix);
  }
  if (HasEdge()) {
    DrawEdge(pGraphics, CFWL_Part::Edge, m_pProperties->m_pThemeProvider,
             pMatrix);
  }
  DrawBkground(pGraphics, m_pProperties->m_pThemeProvider, pMatrix);
  CFX_Matrix matrix;
  matrix.Concat(*pMatrix);

  CFX_WideString wsCaption;
  if (pData)
    pData->GetCaption(this, wsCaption);

  CFX_RectF rtText;
  rtText.Set(0, 0, 0, 0);
  if (!wsCaption.IsEmpty())
    CalcTextRect(wsCaption, pTheme, 0, m_iTTOAlign, rtText);

  switch (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_PSB_ModeMask) {
    case FWL_STYLEEXT_PSB_TextOnly:
      DrawText(pGraphics, m_pProperties->m_pThemeProvider, &matrix);
      break;
    case FWL_STYLEEXT_PSB_IconOnly:
    case FWL_STYLEEXT_PSB_TextIcon:
      break;
  }
}

void IFWL_PushButton::DrawBkground(CFX_Graphics* pGraphics,
                                   IFWL_ThemeProvider* pTheme,
                                   const CFX_Matrix* pMatrix) {
  CFWL_ThemeBackground param;
  param.m_pWidget = this;
  param.m_iPart = CFWL_Part::Background;
  param.m_dwStates = GetPartStates();
  param.m_pGraphics = pGraphics;
  if (pMatrix)
    param.m_matrix.Concat(*pMatrix);
  param.m_rtPart = m_rtClient;
  if (m_pProperties->m_dwStates & FWL_WGTSTATE_Focused)
    param.m_pData = &m_rtCaption;
  pTheme->DrawBackground(&param);
}

void IFWL_PushButton::DrawText(CFX_Graphics* pGraphics,
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
  param.m_dwStates = GetPartStates();
  param.m_pGraphics = pGraphics;
  if (pMatrix)
    param.m_matrix.Concat(*pMatrix);
  param.m_rtPart = m_rtCaption;
  param.m_wsText = wsCaption;
  param.m_dwTTOStyles = m_dwTTOStyles;
  param.m_iTTOAlign = m_iTTOAlign;
  pTheme->DrawText(&param);
}

uint32_t IFWL_PushButton::GetPartStates() {
  uint32_t dwStates = CFWL_PartState_Normal;
  if (m_pProperties->m_dwStates & FWL_WGTSTATE_Focused)
    dwStates |= CFWL_PartState_Focused;
  if (m_pProperties->m_dwStates & FWL_WGTSTATE_Disabled)
    dwStates = CFWL_PartState_Disabled;
  else if (m_pProperties->m_dwStates & FWL_STATE_PSB_Pressed)
    dwStates |= CFWL_PartState_Pressed;
  else if (m_pProperties->m_dwStates & FWL_STATE_PSB_Hovered)
    dwStates |= CFWL_PartState_Hovered;
  else if (m_pProperties->m_dwStates & FWL_STATE_PSB_Default)
    dwStates |= CFWL_PartState_Default;
  return dwStates;
}

void IFWL_PushButton::UpdateTextOutStyles() {
  switch (m_pProperties->m_dwStyleExes &
          (FWL_STYLEEXT_PSB_HLayoutMask | FWL_STYLEEXT_PSB_VLayoutMask)) {
    case FWL_STYLEEXT_PSB_Left | FWL_STYLEEXT_PSB_Top: {
      m_iTTOAlign = FDE_TTOALIGNMENT_TopLeft;
      break;
    }
    case FWL_STYLEEXT_PSB_Center | FWL_STYLEEXT_PSB_Top: {
      m_iTTOAlign = FDE_TTOALIGNMENT_TopCenter;
      break;
    }
    case FWL_STYLEEXT_PSB_Right | FWL_STYLEEXT_PSB_Top: {
      m_iTTOAlign = FDE_TTOALIGNMENT_TopRight;
      break;
    }
    case FWL_STYLEEXT_PSB_Left | FWL_STYLEEXT_PSB_VCenter: {
      m_iTTOAlign = FDE_TTOALIGNMENT_CenterLeft;
      break;
    }
    case FWL_STYLEEXT_PSB_Right | FWL_STYLEEXT_PSB_VCenter: {
      m_iTTOAlign = FDE_TTOALIGNMENT_CenterRight;
      break;
    }
    case FWL_STYLEEXT_PSB_Left | FWL_STYLEEXT_PSB_Bottom: {
      m_iTTOAlign = FDE_TTOALIGNMENT_BottomLeft;
      break;
    }
    case FWL_STYLEEXT_PSB_Center | FWL_STYLEEXT_PSB_Bottom: {
      m_iTTOAlign = FDE_TTOALIGNMENT_BottomCenter;
      break;
    }
    case FWL_STYLEEXT_PSB_Right | FWL_STYLEEXT_PSB_Bottom: {
      m_iTTOAlign = FDE_TTOALIGNMENT_BottomRight;
      break;
    }
    case FWL_STYLEEXT_PSB_Center | FWL_STYLEEXT_PSB_VCenter:
    default: {
      m_iTTOAlign = FDE_TTOALIGNMENT_Center;
      break;
    }
  }
  m_dwTTOStyles = FDE_TTOSTYLE_SingleLine;
  if (m_pProperties->m_dwStyleExes & FWL_WGTSTYLE_RTLReading)
    m_dwTTOStyles |= FDE_TTOSTYLE_RTL;
}

void IFWL_PushButton::OnProcessMessage(CFWL_Message* pMessage) {
  if (!pMessage)
    return;
  if (!IsEnabled())
    return;

  CFWL_MessageType dwMsgCode = pMessage->GetClassID();
  switch (dwMsgCode) {
    case CFWL_MessageType::SetFocus:
      OnFocusChanged(pMessage, true);
      break;
    case CFWL_MessageType::KillFocus:
      OnFocusChanged(pMessage, false);
      break;
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

void IFWL_PushButton::OnDrawWidget(CFX_Graphics* pGraphics,
                                   const CFX_Matrix* pMatrix) {
  DrawWidget(pGraphics, pMatrix);
}

void IFWL_PushButton::OnFocusChanged(CFWL_Message* pMsg, bool bSet) {
  if (bSet)
    m_pProperties->m_dwStates |= FWL_WGTSTATE_Focused;
  else
    m_pProperties->m_dwStates &= ~FWL_WGTSTATE_Focused;

  Repaint(&m_rtClient);
}

void IFWL_PushButton::OnLButtonDown(CFWL_MsgMouse* pMsg) {
  if ((m_pProperties->m_dwStates & FWL_WGTSTATE_Focused) == 0)
    SetFocus(true);

  m_bBtnDown = true;
  m_pProperties->m_dwStates |= FWL_STATE_PSB_Hovered;
  m_pProperties->m_dwStates |= FWL_STATE_PSB_Pressed;
  Repaint(&m_rtClient);
}

void IFWL_PushButton::OnLButtonUp(CFWL_MsgMouse* pMsg) {
  m_bBtnDown = false;
  if (m_rtClient.Contains(pMsg->m_fx, pMsg->m_fy)) {
    m_pProperties->m_dwStates &= ~FWL_STATE_PSB_Pressed;
    m_pProperties->m_dwStates |= FWL_STATE_PSB_Hovered;
  } else {
    m_pProperties->m_dwStates &= ~FWL_STATE_PSB_Hovered;
    m_pProperties->m_dwStates &= ~FWL_STATE_PSB_Pressed;
  }
  if (m_rtClient.Contains(pMsg->m_fx, pMsg->m_fy)) {
    CFWL_EvtClick wmClick;
    wmClick.m_pSrcTarget = this;
    DispatchEvent(&wmClick);
  }
  Repaint(&m_rtClient);
}

void IFWL_PushButton::OnMouseMove(CFWL_MsgMouse* pMsg) {
  bool bRepaint = false;
  if (m_bBtnDown) {
    if (m_rtClient.Contains(pMsg->m_fx, pMsg->m_fy)) {
      if ((m_pProperties->m_dwStates & FWL_STATE_PSB_Pressed) == 0) {
        m_pProperties->m_dwStates |= FWL_STATE_PSB_Pressed;
        bRepaint = true;
      }
      if (m_pProperties->m_dwStates & FWL_STATE_PSB_Hovered) {
        m_pProperties->m_dwStates &= ~FWL_STATE_PSB_Hovered;
        bRepaint = true;
      }
    } else {
      if (m_pProperties->m_dwStates & FWL_STATE_PSB_Pressed) {
        m_pProperties->m_dwStates &= ~FWL_STATE_PSB_Pressed;
        bRepaint = true;
      }
      if ((m_pProperties->m_dwStates & FWL_STATE_PSB_Hovered) == 0) {
        m_pProperties->m_dwStates |= FWL_STATE_PSB_Hovered;
        bRepaint = true;
      }
    }
  } else {
    if (!m_rtClient.Contains(pMsg->m_fx, pMsg->m_fy))
      return;
    if ((m_pProperties->m_dwStates & FWL_STATE_PSB_Hovered) == 0) {
      m_pProperties->m_dwStates |= FWL_STATE_PSB_Hovered;
      bRepaint = true;
    }
  }
  if (bRepaint)
    Repaint(&m_rtClient);
}

void IFWL_PushButton::OnMouseLeave(CFWL_MsgMouse* pMsg) {
  m_bBtnDown = false;
  m_pProperties->m_dwStates &= ~FWL_STATE_PSB_Hovered;
  m_pProperties->m_dwStates &= ~FWL_STATE_PSB_Pressed;
  Repaint(&m_rtClient);
}

void IFWL_PushButton::OnKeyDown(CFWL_MsgKey* pMsg) {
  if (pMsg->m_dwKeyCode == FWL_VKEY_Return) {
    CFWL_EvtMouse wmMouse;
    wmMouse.m_pSrcTarget = this;
    wmMouse.m_dwCmd = FWL_MouseCommand::LeftButtonUp;
    DispatchEvent(&wmMouse);
    CFWL_EvtClick wmClick;
    wmClick.m_pSrcTarget = this;
    DispatchEvent(&wmClick);
    return;
  }
  if (pMsg->m_dwKeyCode != FWL_VKEY_Tab)
    return;

  DispatchKeyEvent(pMsg);
}
