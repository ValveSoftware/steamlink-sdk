// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_IFWL_MONTHCALENDAR_H_
#define XFA_FWL_CORE_IFWL_MONTHCALENDAR_H_

#include "xfa/fgas/localization/fgas_datetime.h"
#include "xfa/fwl/core/cfwl_event.h"
#include "xfa/fwl/core/cfwl_widgetproperties.h"
#include "xfa/fwl/core/ifwl_dataprovider.h"
#include "xfa/fwl/core/ifwl_widget.h"

#define FWL_STYLEEXT_MCD_MultiSelect (1L << 0)
#define FWL_STYLEEXT_MCD_NoToday (1L << 1)
#define FWL_STYLEEXT_MCD_NoTodayCircle (1L << 2)
#define FWL_STYLEEXT_MCD_WeekNumbers (1L << 3)
#define FWL_ITEMSTATE_MCD_Nomal (0L << 0)
#define FWL_ITEMSTATE_MCD_Flag (1L << 0)
#define FWL_ITEMSTATE_MCD_Selected (1L << 1)
#define FWL_ITEMSTATE_MCD_Focused (1L << 2)

class CFWL_MsgMouse;
class IFWL_Widget;

class IFWL_MonthCalendarDP : public IFWL_DataProvider {
 public:
  virtual int32_t GetCurDay(IFWL_Widget* pWidget) = 0;
  virtual int32_t GetCurMonth(IFWL_Widget* pWidget) = 0;
  virtual int32_t GetCurYear(IFWL_Widget* pWidget) = 0;
};

class IFWL_MonthCalendar : public IFWL_Widget {
 public:
  IFWL_MonthCalendar(const IFWL_App* app,
                     std::unique_ptr<CFWL_WidgetProperties> properties,
                     IFWL_Widget* pOuter);
  ~IFWL_MonthCalendar() override;

  // FWL_WidgetImp
  FWL_Type GetClassID() const override;
  void GetWidgetRect(CFX_RectF& rect, bool bAutoSize = false) override;
  void Update() override;
  void DrawWidget(CFX_Graphics* pGraphics,
                  const CFX_Matrix* pMatrix = nullptr) override;
  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnDrawWidget(CFX_Graphics* pGraphics,
                    const CFX_Matrix* pMatrix) override;

  void SetSelect(int32_t iYear, int32_t iMonth, int32_t iDay);

 private:
  struct DATE {
    DATE() : iYear(0), iMonth(0), iDay(0) {}

    DATE(int32_t year, int32_t month, int32_t day)
        : iYear(year), iMonth(month), iDay(day) {}

    bool operator<(const DATE& right) {
      if (iYear < right.iYear)
        return true;
      if (iYear == right.iYear) {
        if (iMonth < right.iMonth)
          return true;
        if (iMonth == right.iMonth)
          return iDay < right.iDay;
      }
      return false;
    }

    bool operator>(const DATE& right) {
      if (iYear > right.iYear)
        return true;
      if (iYear == right.iYear) {
        if (iMonth > right.iMonth)
          return true;
        if (iMonth == right.iMonth)
          return iDay > right.iDay;
      }
      return false;
    }

    int32_t iYear;
    int32_t iMonth;
    int32_t iDay;
  };
  struct DATEINFO {
    DATEINFO(int32_t day,
             int32_t dayofweek,
             uint32_t dwSt,
             CFX_RectF rc,
             CFX_WideString& wsday);
    ~DATEINFO();

    int32_t iDay;
    int32_t iDayOfWeek;
    uint32_t dwStates;
    CFX_RectF rect;
    CFX_WideString wsDay;
  };

  void DrawBackground(CFX_Graphics* pGraphics,
                      IFWL_ThemeProvider* pTheme,
                      const CFX_Matrix* pMatrix);
  void DrawHeadBK(CFX_Graphics* pGraphics,
                  IFWL_ThemeProvider* pTheme,
                  const CFX_Matrix* pMatrix);
  void DrawLButton(CFX_Graphics* pGraphics,
                   IFWL_ThemeProvider* pTheme,
                   const CFX_Matrix* pMatrix);
  void DrawRButton(CFX_Graphics* pGraphics,
                   IFWL_ThemeProvider* pTheme,
                   const CFX_Matrix* pMatrix);
  void DrawCaption(CFX_Graphics* pGraphics,
                   IFWL_ThemeProvider* pTheme,
                   const CFX_Matrix* pMatrix);
  void DrawSeperator(CFX_Graphics* pGraphics,
                     IFWL_ThemeProvider* pTheme,
                     const CFX_Matrix* pMatrix);
  void DrawDatesInBK(CFX_Graphics* pGraphics,
                     IFWL_ThemeProvider* pTheme,
                     const CFX_Matrix* pMatrix);
  void DrawWeek(CFX_Graphics* pGraphics,
                IFWL_ThemeProvider* pTheme,
                const CFX_Matrix* pMatrix);
  void DrawWeekNumber(CFX_Graphics* pGraphics,
                      IFWL_ThemeProvider* pTheme,
                      const CFX_Matrix* pMatrix);
  void DrawWeekNumberSep(CFX_Graphics* pGraphics,
                         IFWL_ThemeProvider* pTheme,
                         const CFX_Matrix* pMatrix);
  void DrawToday(CFX_Graphics* pGraphics,
                 IFWL_ThemeProvider* pTheme,
                 const CFX_Matrix* pMatrix);
  void DrawDatesIn(CFX_Graphics* pGraphics,
                   IFWL_ThemeProvider* pTheme,
                   const CFX_Matrix* pMatrix);
  void DrawDatesOut(CFX_Graphics* pGraphics,
                    IFWL_ThemeProvider* pTheme,
                    const CFX_Matrix* pMatrix);
  void DrawDatesInCircle(CFX_Graphics* pGraphics,
                         IFWL_ThemeProvider* pTheme,
                         const CFX_Matrix* pMatrix);
  CFX_SizeF CalcSize(bool bAutoSize = false);
  void Layout();
  void CalcHeadSize();
  void CalcTodaySize();
  void CalDateItem();
  void GetCapValue();
  void InitDate();
  void ClearDateItem();
  void ResetDateItem();
  void NextMonth();
  void PrevMonth();
  void ChangeToMonth(int32_t iYear, int32_t iMonth);
  void RemoveSelDay(int32_t iDay, bool bAll = false);
  void AddSelDay(int32_t iDay);
  void JumpToToday();
  void GetHeadText(int32_t iYear, int32_t iMonth, CFX_WideString& wsHead);
  void GetTodayText(int32_t iYear,
                    int32_t iMonth,
                    int32_t iDay,
                    CFX_WideString& wsToday);
  int32_t GetDayAtPoint(FX_FLOAT x, FX_FLOAT y);
  void GetDayRect(int32_t iDay, CFX_RectF& rtDay);
  void OnLButtonDown(CFWL_MsgMouse* pMsg);
  void OnLButtonUp(CFWL_MsgMouse* pMsg);
  void DisForm_OnLButtonUp(CFWL_MsgMouse* pMsg);
  void OnMouseMove(CFWL_MsgMouse* pMsg);
  void OnMouseLeave(CFWL_MsgMouse* pMsg);

  bool m_bInitialized;
  CFX_RectF m_rtHead;
  CFX_RectF m_rtWeek;
  CFX_RectF m_rtLBtn;
  CFX_RectF m_rtRBtn;
  CFX_RectF m_rtDates;
  CFX_RectF m_rtHSep;
  CFX_RectF m_rtHeadText;
  CFX_RectF m_rtToday;
  CFX_RectF m_rtTodayFlag;
  CFX_RectF m_rtWeekNum;
  CFX_RectF m_rtWeekNumSep;
  CFX_WideString m_wsHead;
  CFX_WideString m_wsToday;
  std::unique_ptr<CFX_DateTime> m_pDateTime;
  CFX_ArrayTemplate<DATEINFO*> m_arrDates;
  int32_t m_iCurYear;
  int32_t m_iCurMonth;
  int32_t m_iYear;
  int32_t m_iMonth;
  int32_t m_iDay;
  int32_t m_iHovered;
  int32_t m_iLBtnPartStates;
  int32_t m_iRBtnPartStates;
  DATE m_dtMin;
  DATE m_dtMax;
  CFX_SizeF m_szHead;
  CFX_SizeF m_szCell;
  CFX_SizeF m_szToday;
  CFX_ArrayTemplate<int32_t> m_arrSelDays;
  CFX_RectF m_rtClient;
  FX_FLOAT m_fHeadWid;
  FX_FLOAT m_fHeadHei;
  FX_FLOAT m_fHeadBtnWid;
  FX_FLOAT m_fHeadBtnHei;
  FX_FLOAT m_fHeadBtnHMargin;
  FX_FLOAT m_fHeadBtnVMargin;
  FX_FLOAT m_fHeadTextWid;
  FX_FLOAT m_fHeadTextHei;
  FX_FLOAT m_fHeadTextHMargin;
  FX_FLOAT m_fHeadTextVMargin;
  FX_FLOAT m_fHSepWid;
  FX_FLOAT m_fHSepHei;
  FX_FLOAT m_fWeekNumWid;
  FX_FLOAT m_fSepDOffset;
  FX_FLOAT m_fSepX;
  FX_FLOAT m_fSepY;
  FX_FLOAT m_fWeekNumHeigh;
  FX_FLOAT m_fWeekWid;
  FX_FLOAT m_fWeekHei;
  FX_FLOAT m_fDateCellWid;
  FX_FLOAT m_fDateCellHei;
  FX_FLOAT m_fTodayWid;
  FX_FLOAT m_fTodayHei;
  FX_FLOAT m_fTodayFlagWid;
  FX_FLOAT m_fMCWid;
  FX_FLOAT m_fMCHei;
  bool m_bFlag;
};

#endif  // XFA_FWL_CORE_IFWL_MONTHCALENDAR_H_
