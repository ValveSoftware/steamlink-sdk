// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/ifwl_datetimepicker.h"

#include "third_party/base/ptr_util.h"
#include "xfa/fwl/core/cfwl_evteditchanged.h"
#include "xfa/fwl/core/cfwl_msgmouse.h"
#include "xfa/fwl/core/cfwl_msgsetfocus.h"
#include "xfa/fwl/core/cfwl_themebackground.h"
#include "xfa/fwl/core/cfwl_widgetmgr.h"
#include "xfa/fwl/core/fwl_noteimp.h"
#include "xfa/fwl/core/ifwl_formproxy.h"
#include "xfa/fwl/core/ifwl_spinbutton.h"
#include "xfa/fwl/core/ifwl_themeprovider.h"

namespace {

const int kDateTimePickerWidth = 100;
const int kDateTimePickerHeight = 20;

}  // namespace

IFWL_DateTimePicker::IFWL_DateTimePicker(
    const IFWL_App* app,
    std::unique_ptr<CFWL_WidgetProperties> properties)
    : IFWL_Widget(app, std::move(properties), nullptr),
      m_iBtnState(1),
      m_iYear(-1),
      m_iMonth(-1),
      m_iDay(-1),
      m_iCurYear(2010),
      m_iCurMonth(3),
      m_iCurDay(29),
      m_bLBtnDown(false) {
  m_rtBtn.Set(0, 0, 0, 0);

  m_pProperties->m_dwStyleExes = FWL_STYLEEXT_DTP_ShortDateFormat;

  auto monthProp = pdfium::MakeUnique<CFWL_WidgetProperties>(this);
  monthProp->m_dwStyles = FWL_WGTSTYLE_Popup | FWL_WGTSTYLE_Border;
  monthProp->m_dwStates = FWL_WGTSTATE_Invisible;
  monthProp->m_pParent = this;
  monthProp->m_pThemeProvider = m_pProperties->m_pThemeProvider;
  m_pMonthCal.reset(
      new IFWL_MonthCalendar(m_pOwnerApp, std::move(monthProp), this));

  CFX_RectF rtMonthCal;
  m_pMonthCal->GetWidgetRect(rtMonthCal, true);
  rtMonthCal.Set(0, 0, rtMonthCal.width, rtMonthCal.height);
  m_pMonthCal->SetWidgetRect(rtMonthCal);

  auto editProp = pdfium::MakeUnique<CFWL_WidgetProperties>();
  editProp->m_pParent = this;
  editProp->m_pThemeProvider = m_pProperties->m_pThemeProvider;

  m_pEdit.reset(new IFWL_DateTimeEdit(m_pOwnerApp, std::move(editProp), this));
  RegisterEventTarget(m_pMonthCal.get());
  RegisterEventTarget(m_pEdit.get());
}

IFWL_DateTimePicker::~IFWL_DateTimePicker() {
  UnregisterEventTarget();
}

FWL_Type IFWL_DateTimePicker::GetClassID() const {
  return FWL_Type::DateTimePicker;
}

void IFWL_DateTimePicker::GetWidgetRect(CFX_RectF& rect, bool bAutoSize) {
  if (m_pWidgetMgr->IsFormDisabled()) {
    DisForm_GetWidgetRect(rect, bAutoSize);
    return;
  }
  if (!bAutoSize) {
    rect = m_pProperties->m_rtWidget;
    return;
  }

  rect.Set(0, 0, kDateTimePickerWidth, kDateTimePickerHeight);
  IFWL_Widget::GetWidgetRect(rect, true);
}

void IFWL_DateTimePicker::Update() {
  if (m_pWidgetMgr->IsFormDisabled()) {
    DisForm_Update();
    return;
  }
  if (m_iLock)
    return;
  if (!m_pProperties->m_pThemeProvider)
    m_pProperties->m_pThemeProvider = GetAvailableTheme();

  m_pEdit->SetThemeProvider(m_pProperties->m_pThemeProvider);
  GetClientRect(m_rtClient);
  FX_FLOAT* pFWidth = static_cast<FX_FLOAT*>(
      GetThemeCapacity(CFWL_WidgetCapacity::ScrollBarWidth));
  if (!pFWidth)
    return;

  FX_FLOAT fBtn = *pFWidth;
  m_rtBtn.Set(m_rtClient.right() - fBtn, m_rtClient.top, fBtn - 1,
              m_rtClient.height - 1);

  CFX_RectF rtEdit;
  rtEdit.Set(m_rtClient.left, m_rtClient.top, m_rtClient.width - fBtn,
             m_rtClient.height);
  m_pEdit->SetWidgetRect(rtEdit);
  ResetEditAlignment();
  m_pEdit->Update();
  if (!(m_pMonthCal->GetThemeProvider()))
    m_pMonthCal->SetThemeProvider(m_pProperties->m_pThemeProvider);
  if (m_pProperties->m_pDataProvider) {
    IFWL_DateTimePickerDP* pData =
        static_cast<IFWL_DateTimePickerDP*>(m_pProperties->m_pDataProvider);
    pData->GetToday(this, m_iCurYear, m_iCurMonth, m_iCurDay);
  }

  CFX_RectF rtMonthCal;
  m_pMonthCal->GetWidgetRect(rtMonthCal, true);
  CFX_RectF rtPopUp;
  rtPopUp.Set(rtMonthCal.left, rtMonthCal.top + kDateTimePickerHeight,
              rtMonthCal.width, rtMonthCal.height);
  m_pMonthCal->SetWidgetRect(rtPopUp);
  m_pMonthCal->Update();
  return;
}

FWL_WidgetHit IFWL_DateTimePicker::HitTest(FX_FLOAT fx, FX_FLOAT fy) {
  if (m_pWidgetMgr->IsFormDisabled())
    return DisForm_HitTest(fx, fy);
  if (m_rtClient.Contains(fx, fy))
    return FWL_WidgetHit::Client;
  if (IsMonthCalendarVisible()) {
    CFX_RectF rect;
    m_pMonthCal->GetWidgetRect(rect);
    if (rect.Contains(fx, fy))
      return FWL_WidgetHit::Client;
  }
  return FWL_WidgetHit::Unknown;
}

void IFWL_DateTimePicker::DrawWidget(CFX_Graphics* pGraphics,
                                     const CFX_Matrix* pMatrix) {
  if (!pGraphics)
    return;
  if (!m_pProperties->m_pThemeProvider)
    return;

  IFWL_ThemeProvider* pTheme = m_pProperties->m_pThemeProvider;
  if (HasBorder())
    DrawBorder(pGraphics, CFWL_Part::Border, pTheme, pMatrix);
  if (HasEdge())
    DrawEdge(pGraphics, CFWL_Part::Edge, pTheme, pMatrix);
  if (!m_rtBtn.IsEmpty())
    DrawDropDownButton(pGraphics, pTheme, pMatrix);
  if (m_pWidgetMgr->IsFormDisabled()) {
    DisForm_DrawWidget(pGraphics, pMatrix);
    return;
  }
}

void IFWL_DateTimePicker::SetThemeProvider(IFWL_ThemeProvider* pTP) {
  m_pProperties->m_pThemeProvider = pTP;
  m_pMonthCal->SetThemeProvider(pTP);
}

void IFWL_DateTimePicker::GetCurSel(int32_t& iYear,
                                    int32_t& iMonth,
                                    int32_t& iDay) {
  iYear = m_iYear;
  iMonth = m_iMonth;
  iDay = m_iDay;
}

void IFWL_DateTimePicker::SetCurSel(int32_t iYear,
                                    int32_t iMonth,
                                    int32_t iDay) {
  if (iYear <= 0 || iYear >= 3000)
    return;
  if (iMonth <= 0 || iMonth >= 13)
    return;
  if (iDay <= 0 || iDay >= 32)
    return;

  m_iYear = iYear;
  m_iMonth = iMonth;
  m_iDay = iDay;
  m_pMonthCal->SetSelect(iYear, iMonth, iDay);
}

void IFWL_DateTimePicker::SetEditText(const CFX_WideString& wsText) {
  if (!m_pEdit)
    return;

  m_pEdit->SetText(wsText);
  Repaint(&m_rtClient);

  CFWL_EvtEditChanged ev;
  DispatchEvent(&ev);
}

void IFWL_DateTimePicker::GetEditText(CFX_WideString& wsText,
                                      int32_t nStart,
                                      int32_t nCount) const {
  if (m_pEdit)
    m_pEdit->GetText(wsText, nStart, nCount);
}

void IFWL_DateTimePicker::GetBBox(CFX_RectF& rect) const {
  if (m_pWidgetMgr->IsFormDisabled()) {
    DisForm_GetBBox(rect);
    return;
  }

  rect = m_pProperties->m_rtWidget;
  if (IsMonthCalendarVisible()) {
    CFX_RectF rtMonth;
    m_pMonthCal->GetWidgetRect(rtMonth);
    rtMonth.Offset(m_pProperties->m_rtWidget.left,
                   m_pProperties->m_rtWidget.top);
    rect.Union(rtMonth);
  }
}

void IFWL_DateTimePicker::ModifyEditStylesEx(uint32_t dwStylesExAdded,
                                             uint32_t dwStylesExRemoved) {
  m_pEdit->ModifyStylesEx(dwStylesExAdded, dwStylesExRemoved);
}

void IFWL_DateTimePicker::DrawDropDownButton(CFX_Graphics* pGraphics,
                                             IFWL_ThemeProvider* pTheme,
                                             const CFX_Matrix* pMatrix) {
  if ((m_pProperties->m_dwStyleExes & FWL_STYLEEXT_DTP_Spin) ==
      FWL_STYLEEXT_DTP_Spin) {
    return;
  }

  CFWL_ThemeBackground param;
  param.m_pWidget = this;
  param.m_iPart = CFWL_Part::DropDownButton;
  param.m_dwStates = m_iBtnState;
  param.m_pGraphics = pGraphics;
  param.m_rtPart = m_rtBtn;
  if (pMatrix)
    param.m_matrix.Concat(*pMatrix);
  pTheme->DrawBackground(&param);
}

void IFWL_DateTimePicker::FormatDateString(int32_t iYear,
                                           int32_t iMonth,
                                           int32_t iDay,
                                           CFX_WideString& wsText) {
  if ((m_pProperties->m_dwStyleExes & FWL_STYLEEXT_DTP_ShortDateFormat) ==
      FWL_STYLEEXT_DTP_ShortDateFormat) {
    wsText.Format(L"%d-%d-%d", iYear, iMonth, iDay);
  } else if ((m_pProperties->m_dwStyleExes & FWL_STYLEEXT_DTP_LongDateFormat) ==
             FWL_STYLEEXT_DTP_LongDateFormat) {
    wsText.Format(L"%d Year %d Month %d Day", iYear, iMonth, iDay);
  }
}

void IFWL_DateTimePicker::ShowMonthCalendar(bool bActivate) {
  if (m_pWidgetMgr->IsFormDisabled())
    return DisForm_ShowMonthCalendar(bActivate);
  if (IsMonthCalendarVisible() == bActivate)
    return;
  if (!m_pForm)
    InitProxyForm();

  if (!bActivate) {
    m_pForm->EndDoModal();
    return;
  }

  CFX_RectF rtMonth;
  m_pMonthCal->GetWidgetRect(rtMonth);

  CFX_RectF rtAnchor;
  rtAnchor.Set(0, 0, m_pProperties->m_rtWidget.width,
               m_pProperties->m_rtWidget.height);
  GetPopupPos(0, rtMonth.height, rtAnchor, rtMonth);
  m_pForm->SetWidgetRect(rtMonth);

  rtMonth.left = rtMonth.top = 0;
  m_pMonthCal->SetStates(FWL_WGTSTATE_Invisible, !bActivate);
  m_pMonthCal->SetWidgetRect(rtMonth);
  m_pMonthCal->Update();
  m_pForm->DoModal();
}

bool IFWL_DateTimePicker::IsMonthCalendarVisible() const {
  if (m_pWidgetMgr->IsFormDisabled())
    return DisForm_IsMonthCalendarVisible();
  if (!m_pForm)
    return false;
  return !(m_pForm->GetStates() & FWL_WGTSTATE_Invisible);
}

void IFWL_DateTimePicker::ResetEditAlignment() {
  if (!m_pEdit)
    return;

  uint32_t dwAdd = 0;
  switch (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_DTP_EditHAlignMask) {
    case FWL_STYLEEXT_DTP_EditHCenter: {
      dwAdd |= FWL_STYLEEXT_EDT_HCenter;
      break;
    }
    case FWL_STYLEEXT_DTP_EditHFar: {
      dwAdd |= FWL_STYLEEXT_EDT_HFar;
      break;
    }
    default: {
      dwAdd |= FWL_STYLEEXT_EDT_HNear;
      break;
    }
  }
  switch (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_DTP_EditVAlignMask) {
    case FWL_STYLEEXT_DTP_EditVCenter: {
      dwAdd |= FWL_STYLEEXT_EDT_VCenter;
      break;
    }
    case FWL_STYLEEXT_DTP_EditVFar: {
      dwAdd |= FWL_STYLEEXT_EDT_VFar;
      break;
    }
    default: {
      dwAdd |= FWL_STYLEEXT_EDT_VNear;
      break;
    }
  }
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_DTP_EditJustified)
    dwAdd |= FWL_STYLEEXT_EDT_Justified;
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_DTP_EditDistributed)
    dwAdd |= FWL_STYLEEXT_EDT_Distributed;

  m_pEdit->ModifyStylesEx(dwAdd, FWL_STYLEEXT_EDT_HAlignMask |
                                     FWL_STYLEEXT_EDT_HAlignModeMask |
                                     FWL_STYLEEXT_EDT_VAlignMask);
}

void IFWL_DateTimePicker::ProcessSelChanged(int32_t iYear,
                                            int32_t iMonth,
                                            int32_t iDay) {
  m_iYear = iYear;
  m_iMonth = iMonth;
  m_iDay = iDay;

  CFX_WideString wsText;
  FormatDateString(m_iYear, m_iMonth, m_iDay, wsText);
  m_pEdit->SetText(wsText);
  m_pEdit->Update();
  Repaint(&m_rtClient);

  CFWL_Event_DtpSelectChanged ev;
  ev.m_pSrcTarget = this;
  ev.iYear = m_iYear;
  ev.iMonth = m_iMonth;
  ev.iDay = m_iDay;
  DispatchEvent(&ev);
}

void IFWL_DateTimePicker::InitProxyForm() {
  if (m_pForm)
    return;
  if (!m_pMonthCal)
    return;

  auto prop = pdfium::MakeUnique<CFWL_WidgetProperties>();
  prop->m_dwStyles = FWL_WGTSTYLE_Popup;
  prop->m_dwStates = FWL_WGTSTATE_Invisible;
  prop->m_pOwner = this;

  m_pForm = pdfium::MakeUnique<IFWL_FormProxy>(m_pOwnerApp, std::move(prop),
                                               m_pMonthCal.get());
  m_pMonthCal->SetParent(m_pForm.get());
}

bool IFWL_DateTimePicker::DisForm_IsMonthCalendarVisible() const {
  if (!m_pMonthCal)
    return false;
  return !(m_pMonthCal->GetStates() & FWL_WGTSTATE_Invisible);
}

void IFWL_DateTimePicker::DisForm_ShowMonthCalendar(bool bActivate) {
  if (IsMonthCalendarVisible() == bActivate)
    return;

  if (bActivate) {
    CFX_RectF rtMonthCal;
    m_pMonthCal->GetWidgetRect(rtMonthCal, true);
    FX_FLOAT fPopupMin = rtMonthCal.height;
    FX_FLOAT fPopupMax = rtMonthCal.height;
    CFX_RectF rtAnchor(m_pProperties->m_rtWidget);
    rtAnchor.width = rtMonthCal.width;
    rtMonthCal.left = m_rtClient.left;
    rtMonthCal.top = rtAnchor.Height();
    GetPopupPos(fPopupMin, fPopupMax, rtAnchor, rtMonthCal);
    m_pMonthCal->SetWidgetRect(rtMonthCal);
    if (m_iYear > 0 && m_iMonth > 0 && m_iDay > 0)
      m_pMonthCal->SetSelect(m_iYear, m_iMonth, m_iDay);
    m_pMonthCal->Update();
  }
  m_pMonthCal->SetStates(FWL_WGTSTATE_Invisible, !bActivate);

  if (bActivate) {
    CFWL_MsgSetFocus msg;
    msg.m_pDstTarget = m_pMonthCal.get();
    msg.m_pSrcTarget = m_pEdit.get();
    m_pEdit->GetDelegate()->OnProcessMessage(&msg);
  }

  CFX_RectF rtInvalidate, rtCal;
  rtInvalidate.Set(0, 0, m_pProperties->m_rtWidget.width,
                   m_pProperties->m_rtWidget.height);
  m_pMonthCal->GetWidgetRect(rtCal);
  rtInvalidate.Union(rtCal);
  rtInvalidate.Inflate(2, 2);
  Repaint(&rtInvalidate);
}

FWL_WidgetHit IFWL_DateTimePicker::DisForm_HitTest(FX_FLOAT fx,
                                                   FX_FLOAT fy) const {
  CFX_RectF rect;
  rect.Set(0, 0, m_pProperties->m_rtWidget.width,
           m_pProperties->m_rtWidget.height);
  if (rect.Contains(fx, fy))
    return FWL_WidgetHit::Edit;
  if (DisForm_IsNeedShowButton())
    rect.width += m_fBtn;
  if (rect.Contains(fx, fy))
    return FWL_WidgetHit::Client;
  if (IsMonthCalendarVisible()) {
    m_pMonthCal->GetWidgetRect(rect);
    if (rect.Contains(fx, fy))
      return FWL_WidgetHit::Client;
  }
  return FWL_WidgetHit::Unknown;
}

bool IFWL_DateTimePicker::DisForm_IsNeedShowButton() const {
  return m_pProperties->m_dwStates & FWL_WGTSTATE_Focused ||
         m_pMonthCal->GetStates() & FWL_WGTSTATE_Focused ||
         m_pEdit->GetStates() & FWL_WGTSTATE_Focused;
}

void IFWL_DateTimePicker::DisForm_Update() {
  if (m_iLock)
    return;
  if (!m_pProperties->m_pThemeProvider)
    m_pProperties->m_pThemeProvider = GetAvailableTheme();

  m_pEdit->SetThemeProvider(m_pProperties->m_pThemeProvider);
  GetClientRect(m_rtClient);
  m_pEdit->SetWidgetRect(m_rtClient);
  ResetEditAlignment();
  m_pEdit->Update();

  if (!m_pMonthCal->GetThemeProvider())
    m_pMonthCal->SetThemeProvider(m_pProperties->m_pThemeProvider);
  if (m_pProperties->m_pDataProvider) {
    IFWL_DateTimePickerDP* pData =
        static_cast<IFWL_DateTimePickerDP*>(m_pProperties->m_pDataProvider);
    pData->GetToday(this, m_iCurYear, m_iCurMonth, m_iCurDay);
  }

  FX_FLOAT* pWidth = static_cast<FX_FLOAT*>(
      GetThemeCapacity(CFWL_WidgetCapacity::ScrollBarWidth));
  if (!pWidth)
    return;

  m_fBtn = *pWidth;
  CFX_RectF rtMonthCal;
  m_pMonthCal->GetWidgetRect(rtMonthCal, true);

  CFX_RectF rtPopUp;
  rtPopUp.Set(rtMonthCal.left, rtMonthCal.top + kDateTimePickerHeight,
              rtMonthCal.width, rtMonthCal.height);
  m_pMonthCal->SetWidgetRect(rtPopUp);
  m_pMonthCal->Update();
}

void IFWL_DateTimePicker::DisForm_GetWidgetRect(CFX_RectF& rect,
                                                bool bAutoSize) {
  rect = m_pProperties->m_rtWidget;
  if (DisForm_IsNeedShowButton())
    rect.width += m_fBtn;
}

void IFWL_DateTimePicker::DisForm_GetBBox(CFX_RectF& rect) const {
  rect = m_pProperties->m_rtWidget;
  if (DisForm_IsNeedShowButton())
    rect.width += m_fBtn;
  if (!IsMonthCalendarVisible())
    return;

  CFX_RectF rtMonth;
  m_pMonthCal->GetWidgetRect(rtMonth);
  rtMonth.Offset(m_pProperties->m_rtWidget.left, m_pProperties->m_rtWidget.top);
  rect.Union(rtMonth);
}

void IFWL_DateTimePicker::DisForm_DrawWidget(CFX_Graphics* pGraphics,
                                             const CFX_Matrix* pMatrix) {
  if (!pGraphics)
    return;
  if (m_pEdit) {
    CFX_RectF rtEdit;
    m_pEdit->GetWidgetRect(rtEdit);

    CFX_Matrix mt;
    mt.Set(1, 0, 0, 1, rtEdit.left, rtEdit.top);
    if (pMatrix)
      mt.Concat(*pMatrix);
    m_pEdit->DrawWidget(pGraphics, &mt);
  }
  if (!IsMonthCalendarVisible())
    return;

  CFX_RectF rtMonth;
  m_pMonthCal->GetWidgetRect(rtMonth);
  CFX_Matrix mt;
  mt.Set(1, 0, 0, 1, rtMonth.left, rtMonth.top);
  if (pMatrix)
    mt.Concat(*pMatrix);
  m_pMonthCal->DrawWidget(pGraphics, &mt);
}

void IFWL_DateTimePicker::OnProcessMessage(CFWL_Message* pMessage) {
  if (!pMessage)
    return;

  switch (pMessage->GetClassID()) {
    case CFWL_MessageType::SetFocus:
      OnFocusChanged(pMessage, true);
      break;
    case CFWL_MessageType::KillFocus:
      OnFocusChanged(pMessage, false);
      break;
    case CFWL_MessageType::Mouse: {
      CFWL_MsgMouse* pMouse = static_cast<CFWL_MsgMouse*>(pMessage);
      switch (pMouse->m_dwCmd) {
        case FWL_MouseCommand::LeftButtonDown:
          OnLButtonDown(pMouse);
          break;
        case FWL_MouseCommand::LeftButtonUp:
          OnLButtonUp(pMouse);
          break;
        case FWL_MouseCommand::Move:
          OnMouseMove(pMouse);
          break;
        case FWL_MouseCommand::Leave:
          OnMouseLeave(pMouse);
          break;
        default:
          break;
      }
      break;
    }
    case CFWL_MessageType::Key: {
      if (m_pEdit->GetStates() & FWL_WGTSTATE_Focused) {
        m_pEdit->GetDelegate()->OnProcessMessage(pMessage);
        return;
      }
      break;
    }
    default:
      break;
  }

  IFWL_Widget::OnProcessMessage(pMessage);
}

void IFWL_DateTimePicker::OnDrawWidget(CFX_Graphics* pGraphics,
                                       const CFX_Matrix* pMatrix) {
  DrawWidget(pGraphics, pMatrix);
}

void IFWL_DateTimePicker::OnFocusChanged(CFWL_Message* pMsg, bool bSet) {
  if (!pMsg)
    return;
  if (m_pWidgetMgr->IsFormDisabled())
    return DisForm_OnFocusChanged(pMsg, bSet);

  if (bSet) {
    m_pProperties->m_dwStates |= (FWL_WGTSTATE_Focused);
    Repaint(&m_rtClient);
  } else {
    m_pProperties->m_dwStates &= ~(FWL_WGTSTATE_Focused);
    Repaint(&m_rtClient);
  }
  if (pMsg->m_pSrcTarget == m_pMonthCal.get() && IsMonthCalendarVisible()) {
    ShowMonthCalendar(false);
  }
  Repaint(&m_rtClient);
}

void IFWL_DateTimePicker::OnLButtonDown(CFWL_MsgMouse* pMsg) {
  if (!pMsg)
    return;
  if ((m_pProperties->m_dwStates & FWL_WGTSTATE_Focused) == 0)
    SetFocus(true);
  if (!m_rtBtn.Contains(pMsg->m_fx, pMsg->m_fy))
    return;

  if (IsMonthCalendarVisible()) {
    ShowMonthCalendar(false);
    return;
  }
  if (!(m_pProperties->m_dwStyleExes & FWL_STYLEEXT_DTP_TimeFormat))
    ShowMonthCalendar(true);

  m_bLBtnDown = true;
  Repaint(&m_rtClient);
}

void IFWL_DateTimePicker::OnLButtonUp(CFWL_MsgMouse* pMsg) {
  if (!pMsg)
    return;

  m_bLBtnDown = false;
  if (m_rtBtn.Contains(pMsg->m_fx, pMsg->m_fy))
    m_iBtnState = CFWL_PartState_Hovered;
  else
    m_iBtnState = CFWL_PartState_Normal;
  Repaint(&m_rtBtn);
}

void IFWL_DateTimePicker::OnMouseMove(CFWL_MsgMouse* pMsg) {
  if (!m_rtBtn.Contains(pMsg->m_fx, pMsg->m_fy))
    m_iBtnState = CFWL_PartState_Normal;
  Repaint(&m_rtBtn);
}

void IFWL_DateTimePicker::OnMouseLeave(CFWL_MsgMouse* pMsg) {
  if (!pMsg)
    return;
  m_iBtnState = CFWL_PartState_Normal;
  Repaint(&m_rtBtn);
}

void IFWL_DateTimePicker::DisForm_OnFocusChanged(CFWL_Message* pMsg,
                                                 bool bSet) {
  CFX_RectF rtInvalidate(m_rtBtn);
  if (bSet) {
    m_pProperties->m_dwStates |= FWL_WGTSTATE_Focused;
    if (m_pEdit && !(m_pEdit->GetStylesEx() & FWL_STYLEEXT_EDT_ReadOnly)) {
      m_rtBtn.Set(m_pProperties->m_rtWidget.width, 0, m_fBtn,
                  m_pProperties->m_rtWidget.height - 1);
    }
    rtInvalidate = m_rtBtn;
    pMsg->m_pDstTarget = m_pEdit.get();
    m_pEdit->GetDelegate()->OnProcessMessage(pMsg);
  } else {
    m_pProperties->m_dwStates &= ~FWL_WGTSTATE_Focused;
    m_rtBtn.Set(0, 0, 0, 0);
    if (DisForm_IsMonthCalendarVisible())
      ShowMonthCalendar(false);
    if (m_pEdit->GetStates() & FWL_WGTSTATE_Focused) {
      pMsg->m_pSrcTarget = m_pEdit.get();
      m_pEdit->GetDelegate()->OnProcessMessage(pMsg);
    }
  }
  rtInvalidate.Inflate(2, 2);
  Repaint(&rtInvalidate);
}

void IFWL_DateTimePicker::GetCaption(IFWL_Widget* pWidget,
                                     CFX_WideString& wsCaption) {}

int32_t IFWL_DateTimePicker::GetCurDay(IFWL_Widget* pWidget) {
  return m_iCurDay;
}

int32_t IFWL_DateTimePicker::GetCurMonth(IFWL_Widget* pWidget) {
  return m_iCurMonth;
}

int32_t IFWL_DateTimePicker::GetCurYear(IFWL_Widget* pWidget) {
  return m_iCurYear;
}
