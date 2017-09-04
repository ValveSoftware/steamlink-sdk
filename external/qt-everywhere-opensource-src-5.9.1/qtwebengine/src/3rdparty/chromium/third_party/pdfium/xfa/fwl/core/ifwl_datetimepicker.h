// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_IFWL_DATETIMEPICKER_H_
#define XFA_FWL_CORE_IFWL_DATETIMEPICKER_H_

#include "xfa/fwl/core/cfwl_event.h"
#include "xfa/fwl/core/cfwl_widgetproperties.h"
#include "xfa/fwl/core/ifwl_dataprovider.h"
#include "xfa/fwl/core/ifwl_datetimeedit.h"
#include "xfa/fwl/core/ifwl_monthcalendar.h"
#include "xfa/fwl/core/ifwl_widget.h"

#define FWL_STYLEEXT_DTP_AllowEdit (1L << 0)
#define FWL_STYLEEXT_DTP_LongDateFormat (0L << 1)
#define FWL_STYLEEXT_DTP_ShortDateFormat (1L << 1)
#define FWL_STYLEEXT_DTP_TimeFormat (2L << 1)
#define FWL_STYLEEXT_DTP_Spin (1L << 3)
#define FWL_STYLEEXT_DTP_EditHNear (0L << 4)
#define FWL_STYLEEXT_DTP_EditHCenter (1L << 4)
#define FWL_STYLEEXT_DTP_EditHFar (2L << 4)
#define FWL_STYLEEXT_DTP_EditVNear (0L << 6)
#define FWL_STYLEEXT_DTP_EditVCenter (1L << 6)
#define FWL_STYLEEXT_DTP_EditVFar (2L << 6)
#define FWL_STYLEEXT_DTP_EditJustified (1L << 8)
#define FWL_STYLEEXT_DTP_EditDistributed (2L << 8)
#define FWL_STYLEEXT_DTP_EditHAlignMask (3L << 4)
#define FWL_STYLEEXT_DTP_EditVAlignMask (3L << 6)
#define FWL_STYLEEXT_DTP_EditHAlignModeMask (3L << 8)

class IFWL_DateTimeEdit;
class IFWL_FormProxy;

FWL_EVENT_DEF(CFWL_Event_DtpSelectChanged,
              CFWL_EventType::SelectChanged,
              int32_t iYear;
              int32_t iMonth;
              int32_t iDay;)

class IFWL_DateTimePickerDP : public IFWL_DataProvider {
 public:
  virtual void GetToday(IFWL_Widget* pWidget,
                        int32_t& iYear,
                        int32_t& iMonth,
                        int32_t& iDay) = 0;
};

class IFWL_DateTimePicker : public IFWL_Widget, public IFWL_MonthCalendarDP {
 public:
  explicit IFWL_DateTimePicker(
      const IFWL_App* app,
      std::unique_ptr<CFWL_WidgetProperties> properties);
  ~IFWL_DateTimePicker() override;

  // IFWL_Widget
  FWL_Type GetClassID() const override;
  void GetWidgetRect(CFX_RectF& rect, bool bAutoSize = false) override;
  void Update() override;
  FWL_WidgetHit HitTest(FX_FLOAT fx, FX_FLOAT fy) override;
  void DrawWidget(CFX_Graphics* pGraphics,
                  const CFX_Matrix* pMatrix = nullptr) override;
  void SetThemeProvider(IFWL_ThemeProvider* pTP) override;
  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnDrawWidget(CFX_Graphics* pGraphics,
                    const CFX_Matrix* pMatrix) override;

  // IFWL_DataProvider
  void GetCaption(IFWL_Widget* pWidget, CFX_WideString& wsCaption) override;

  // IFWL_MonthCalendarDP
  int32_t GetCurDay(IFWL_Widget* pWidget) override;
  int32_t GetCurMonth(IFWL_Widget* pWidget) override;
  int32_t GetCurYear(IFWL_Widget* pWidget) override;

  void GetCurSel(int32_t& iYear, int32_t& iMonth, int32_t& iDay);
  void SetCurSel(int32_t iYear, int32_t iMonth, int32_t iDay);

  void SetEditText(const CFX_WideString& wsText);
  void GetEditText(CFX_WideString& wsText,
                   int32_t nStart = 0,
                   int32_t nCount = -1) const;

  int32_t CountSelRanges() const { return m_pEdit->CountSelRanges(); }
  int32_t GetSelRange(int32_t nIndex, int32_t& nStart) const {
    return m_pEdit->GetSelRange(nIndex, nStart);
  }

  void GetBBox(CFX_RectF& rect) const;
  void SetEditLimit(int32_t nLimit) { m_pEdit->SetLimit(nLimit); }
  void ModifyEditStylesEx(uint32_t dwStylesExAdded, uint32_t dwStylesExRemoved);

  bool IsMonthCalendarVisible() const;
  void ShowMonthCalendar(bool bActivate);
  void ProcessSelChanged(int32_t iYear, int32_t iMonth, int32_t iDay);

  IFWL_FormProxy* GetFormProxy() const { return m_pForm.get(); }

 private:
  void DrawDropDownButton(CFX_Graphics* pGraphics,
                          IFWL_ThemeProvider* pTheme,
                          const CFX_Matrix* pMatrix);
  void FormatDateString(int32_t iYear,
                        int32_t iMonth,
                        int32_t iDay,
                        CFX_WideString& wsText);
  void ResetEditAlignment();
  void InitProxyForm();

  bool DisForm_IsMonthCalendarVisible() const;
  void DisForm_ShowMonthCalendar(bool bActivate);
  FWL_WidgetHit DisForm_HitTest(FX_FLOAT fx, FX_FLOAT fy) const;
  bool DisForm_IsNeedShowButton() const;
  void DisForm_Update();
  void DisForm_GetWidgetRect(CFX_RectF& rect, bool bAutoSize = false);
  void DisForm_GetBBox(CFX_RectF& rect) const;
  void DisForm_DrawWidget(CFX_Graphics* pGraphics,
                          const CFX_Matrix* pMatrix = nullptr);
  void DisForm_OnFocusChanged(CFWL_Message* pMsg, bool bSet);
  void OnFocusChanged(CFWL_Message* pMsg, bool bSet);
  void OnLButtonDown(CFWL_MsgMouse* pMsg);
  void OnLButtonUp(CFWL_MsgMouse* pMsg);
  void OnMouseMove(CFWL_MsgMouse* pMsg);
  void OnMouseLeave(CFWL_MsgMouse* pMsg);

  CFX_RectF m_rtBtn;
  CFX_RectF m_rtClient;
  int32_t m_iBtnState;
  int32_t m_iYear;
  int32_t m_iMonth;
  int32_t m_iDay;
  int32_t m_iCurYear;
  int32_t m_iCurMonth;
  int32_t m_iCurDay;
  bool m_bLBtnDown;
  std::unique_ptr<IFWL_DateTimeEdit> m_pEdit;
  std::unique_ptr<IFWL_MonthCalendar> m_pMonthCal;
  std::unique_ptr<IFWL_FormProxy> m_pForm;
  FX_FLOAT m_fBtn;
};

#endif  // XFA_FWL_CORE_IFWL_DATETIMEPICKER_H_
