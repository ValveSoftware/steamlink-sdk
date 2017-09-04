// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/ifwl_comboboxproxy.h"

#include "xfa/fwl/core/cfwl_msgkillfocus.h"
#include "xfa/fwl/core/cfwl_msgmouse.h"
#include "xfa/fwl/core/fwl_noteimp.h"
#include "xfa/fwl/core/ifwl_app.h"
#include "xfa/fwl/core/ifwl_combobox.h"

IFWL_ComboBoxProxy::IFWL_ComboBoxProxy(
    IFWL_ComboBox* pComboBox,
    const IFWL_App* app,
    std::unique_ptr<CFWL_WidgetProperties> properties,
    IFWL_Widget* pOuter)
    : IFWL_FormProxy(app, std::move(properties), pOuter),
      m_bLButtonDown(false),
      m_bLButtonUpSelf(false),
      m_pComboBox(pComboBox) {}

IFWL_ComboBoxProxy::~IFWL_ComboBoxProxy() {}

void IFWL_ComboBoxProxy::OnProcessMessage(CFWL_Message* pMessage) {
  if (!pMessage)
    return;

  switch (pMessage->GetClassID()) {
    case CFWL_MessageType::Mouse: {
      CFWL_MsgMouse* pMsg = static_cast<CFWL_MsgMouse*>(pMessage);
      switch (pMsg->m_dwCmd) {
        case FWL_MouseCommand::LeftButtonDown:
          OnLButtonDown(pMsg);
          break;
        case FWL_MouseCommand::LeftButtonUp:
          OnLButtonUp(pMsg);
          break;
        default:
          break;
      }
      break;
    }
    case CFWL_MessageType::KillFocus:
      OnFocusChanged(pMessage, false);
      break;
    case CFWL_MessageType::SetFocus:
      OnFocusChanged(pMessage, true);
      break;
    default:
      break;
  }
  IFWL_Widget::OnProcessMessage(pMessage);
}

void IFWL_ComboBoxProxy::OnDrawWidget(CFX_Graphics* pGraphics,
                                      const CFX_Matrix* pMatrix) {
  m_pComboBox->DrawStretchHandler(pGraphics, pMatrix);
}

void IFWL_ComboBoxProxy::OnLButtonDown(CFWL_Message* pMessage) {
  const IFWL_App* pApp = GetOwnerApp();
  if (!pApp)
    return;

  CFWL_NoteDriver* pDriver =
      static_cast<CFWL_NoteDriver*>(pApp->GetNoteDriver());
  CFX_RectF rtWidget;
  GetWidgetRect(rtWidget);
  rtWidget.left = rtWidget.top = 0;

  CFWL_MsgMouse* pMsg = static_cast<CFWL_MsgMouse*>(pMessage);
  if (rtWidget.Contains(pMsg->m_fx, pMsg->m_fy)) {
    m_bLButtonDown = true;
    pDriver->SetGrab(this, true);
  } else {
    m_bLButtonDown = false;
    pDriver->SetGrab(this, false);
    m_pComboBox->ShowDropList(false);
  }
}

void IFWL_ComboBoxProxy::OnLButtonUp(CFWL_Message* pMessage) {
  m_bLButtonDown = false;
  const IFWL_App* pApp = GetOwnerApp();
  if (!pApp)
    return;

  CFWL_NoteDriver* pDriver =
      static_cast<CFWL_NoteDriver*>(pApp->GetNoteDriver());
  pDriver->SetGrab(this, false);
  if (!m_bLButtonUpSelf) {
    m_bLButtonUpSelf = true;
    return;
  }

  CFWL_MsgMouse* pMsg = static_cast<CFWL_MsgMouse*>(pMessage);
  CFX_RectF rect;
  GetWidgetRect(rect);
  rect.left = rect.top = 0;
  if (!rect.Contains(pMsg->m_fx, pMsg->m_fy) &&
      m_pComboBox->IsDropListVisible()) {
    m_pComboBox->ShowDropList(false);
  }
}

void IFWL_ComboBoxProxy::OnFocusChanged(CFWL_Message* pMessage, bool bSet) {
  if (bSet)
    return;

  CFWL_MsgKillFocus* pMsg = static_cast<CFWL_MsgKillFocus*>(pMessage);
  if (!pMsg->m_pSetFocus)
    m_pComboBox->ShowDropList(false);
}
