// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/ifwl_comboedit.h"

#include "xfa/fde/cfde_txtedtengine.h"
#include "xfa/fwl/core/cfwl_msgmouse.h"
#include "xfa/fwl/core/ifwl_combobox.h"

IFWL_ComboEdit::IFWL_ComboEdit(
    const IFWL_App* app,
    std::unique_ptr<CFWL_WidgetProperties> properties,
    IFWL_Widget* pOuter)
    : IFWL_Edit(app, std::move(properties), pOuter) {
  m_pOuter = static_cast<IFWL_ComboBox*>(pOuter);
}

void IFWL_ComboEdit::ClearSelected() {
  ClearSelections();
  Repaint(&GetRTClient());
}

void IFWL_ComboEdit::SetSelected() {
  FlagFocus(true);
  GetTxtEdtEngine()->MoveCaretPos(MC_End);
  AddSelRange(0);
}

void IFWL_ComboEdit::FlagFocus(bool bSet) {
  if (bSet) {
    m_pProperties->m_dwStates |= FWL_WGTSTATE_Focused;
    return;
  }

  m_pProperties->m_dwStates &= ~FWL_WGTSTATE_Focused;
  ShowCaret(false);
}

void IFWL_ComboEdit::OnProcessMessage(CFWL_Message* pMessage) {
  if (!pMessage)
    return;

  bool backDefault = true;
  switch (pMessage->GetClassID()) {
    case CFWL_MessageType::SetFocus: {
      m_pProperties->m_dwStates |= FWL_WGTSTATE_Focused;
      backDefault = false;
      break;
    }
    case CFWL_MessageType::KillFocus: {
      m_pProperties->m_dwStates &= ~FWL_WGTSTATE_Focused;
      backDefault = false;
      break;
    }
    case CFWL_MessageType::Mouse: {
      CFWL_MsgMouse* pMsg = static_cast<CFWL_MsgMouse*>(pMessage);
      if ((pMsg->m_dwCmd == FWL_MouseCommand::LeftButtonDown) &&
          ((m_pProperties->m_dwStates & FWL_WGTSTATE_Focused) == 0)) {
        SetSelected();
        m_pOuter->SetFocus(true);
      }
      break;
    }
    default:
      break;
  }
  if (backDefault)
    IFWL_Edit::OnProcessMessage(pMessage);
}
