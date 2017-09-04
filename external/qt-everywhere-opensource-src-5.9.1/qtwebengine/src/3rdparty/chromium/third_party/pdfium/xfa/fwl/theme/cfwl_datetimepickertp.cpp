// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/theme/cfwl_datetimepickertp.h"

#include "xfa/fwl/core/cfwl_themebackground.h"
#include "xfa/fwl/core/ifwl_datetimepicker.h"

CFWL_DateTimePickerTP::CFWL_DateTimePickerTP() {}

CFWL_DateTimePickerTP::~CFWL_DateTimePickerTP() {}

bool CFWL_DateTimePickerTP::IsValidWidget(IFWL_Widget* pWidget) {
  return pWidget && pWidget->GetClassID() == FWL_Type::DateTimePicker;
}

void CFWL_DateTimePickerTP::DrawBackground(CFWL_ThemeBackground* pParams) {
  if (!pParams)
    return;

  switch (pParams->m_iPart) {
    case CFWL_Part::Border: {
      DrawBorder(pParams->m_pGraphics, &pParams->m_rtPart, &pParams->m_matrix);
      break;
    }
    case CFWL_Part::Edge: {
      DrawEdge(pParams->m_pGraphics, pParams->m_pWidget->GetStyles(),
               &pParams->m_rtPart, &pParams->m_matrix);
      break;
    }
    case CFWL_Part::DropDownButton: {
      DrawDropDownButton(pParams, &pParams->m_matrix);
      break;
    }
    default:
      break;
  }
}

void CFWL_DateTimePickerTP::DrawDropDownButton(CFWL_ThemeBackground* pParams,
                                               CFX_Matrix* pMatrix) {
  uint32_t dwStates = pParams->m_dwStates;
  dwStates &= 0x03;
  FWLTHEME_STATE eState = FWLTHEME_STATE_Normal;
  switch (eState & dwStates) {
    case CFWL_PartState_Normal: {
      eState = FWLTHEME_STATE_Normal;
      break;
    }
    case CFWL_PartState_Hovered: {
      eState = FWLTHEME_STATE_Hover;
      break;
    }
    case CFWL_PartState_Pressed: {
      eState = FWLTHEME_STATE_Pressed;
      break;
    }
    case CFWL_PartState_Disabled: {
      eState = FWLTHEME_STATE_Disable;
      break;
    }
    default:
      break;
  }
  DrawArrowBtn(pParams->m_pGraphics, &pParams->m_rtPart,
               FWLTHEME_DIRECTION_Down, eState, pMatrix);
}

