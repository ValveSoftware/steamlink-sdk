// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/ifwl_datetimeedit.h"

#include "third_party/base/ptr_util.h"
#include "xfa/fwl/core/cfwl_msgmouse.h"
#include "xfa/fwl/core/cfwl_widgetmgr.h"
#include "xfa/fwl/core/ifwl_datetimepicker.h"

IFWL_DateTimeEdit::IFWL_DateTimeEdit(
    const IFWL_App* app,
    std::unique_ptr<CFWL_WidgetProperties> properties,
    IFWL_Widget* pOuter)
    : IFWL_Edit(app, std::move(properties), pOuter) {}

void IFWL_DateTimeEdit::OnProcessMessage(CFWL_Message* pMessage) {
  if (m_pWidgetMgr->IsFormDisabled()) {
    DisForm_OnProcessMessage(pMessage);
    return;
  }

  CFWL_MessageType dwHashCode = pMessage->GetClassID();
  if (dwHashCode == CFWL_MessageType::SetFocus ||
      dwHashCode == CFWL_MessageType::KillFocus) {
    IFWL_Widget* pOuter = GetOuter();
    pOuter->GetDelegate()->OnProcessMessage(pMessage);
  }
}

void IFWL_DateTimeEdit::DisForm_OnProcessMessage(CFWL_Message* pMessage) {
  CFWL_MessageType dwHashCode = pMessage->GetClassID();
  if (!m_pWidgetMgr->IsFormDisabled() ||
      dwHashCode != CFWL_MessageType::Mouse) {
    IFWL_Edit::OnProcessMessage(pMessage);
    return;
  }

  CFWL_MsgMouse* pMouse = static_cast<CFWL_MsgMouse*>(pMessage);
  if (pMouse->m_dwCmd == FWL_MouseCommand::LeftButtonDown ||
      pMouse->m_dwCmd == FWL_MouseCommand::RightButtonDown) {
    if ((m_pProperties->m_dwStates & FWL_WGTSTATE_Focused) == 0)
      m_pProperties->m_dwStates |= FWL_WGTSTATE_Focused;

    IFWL_DateTimePicker* pDateTime =
        static_cast<IFWL_DateTimePicker*>(m_pOuter);
    if (pDateTime->IsMonthCalendarVisible()) {
      CFX_RectF rtInvalidate;
      pDateTime->GetWidgetRect(rtInvalidate);
      pDateTime->ShowMonthCalendar(false);
      rtInvalidate.Offset(-rtInvalidate.left, -rtInvalidate.top);
      pDateTime->Repaint(&rtInvalidate);
    }
  }
  IFWL_Edit::OnProcessMessage(pMessage);
}
