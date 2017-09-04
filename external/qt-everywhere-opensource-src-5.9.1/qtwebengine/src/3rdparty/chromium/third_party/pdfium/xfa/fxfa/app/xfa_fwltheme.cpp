// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/app/xfa_fwltheme.h"

#include "xfa/fde/tto/fde_textout.h"
#include "xfa/fgas/crt/fgas_codepage.h"
#include "xfa/fgas/font/fgas_gefont.h"
#include "xfa/fwl/core/cfwl_themebackground.h"
#include "xfa/fwl/core/cfwl_themetext.h"
#include "xfa/fwl/core/ifwl_barcode.h"
#include "xfa/fwl/core/ifwl_caret.h"
#include "xfa/fwl/core/ifwl_checkbox.h"
#include "xfa/fwl/core/ifwl_combobox.h"
#include "xfa/fwl/core/ifwl_datetimepicker.h"
#include "xfa/fwl/core/ifwl_edit.h"
#include "xfa/fwl/core/ifwl_listbox.h"
#include "xfa/fwl/core/ifwl_monthcalendar.h"
#include "xfa/fwl/core/ifwl_picturebox.h"
#include "xfa/fwl/core/ifwl_pushbutton.h"
#include "xfa/fwl/core/ifwl_scrollbar.h"
#include "xfa/fxfa/xfa_ffapp.h"
#include "xfa/fxfa/xfa_ffwidget.h"
#include "xfa/fxgraphics/cfx_color.h"

namespace {

const FX_WCHAR* const g_FWLTheme_CalFonts[] = {
    L"Arial", L"Courier New", L"DejaVu Sans",
};

}  // namespace

CXFA_FFWidget* XFA_ThemeGetOuterWidget(IFWL_Widget* pWidget) {
  IFWL_Widget* pOuter = pWidget;
  while (pOuter && pOuter->GetOuter())
    pOuter = pOuter->GetOuter();

  return pOuter ? static_cast<CXFA_FFWidget*>(pOuter->GetLayoutItem())
                : nullptr;
}

CXFA_FWLTheme::CXFA_FWLTheme(CXFA_FFApp* pApp)
    : m_pCheckBoxTP(new CFWL_CheckBoxTP),
      m_pListBoxTP(new CFWL_ListBoxTP),
      m_pPictureBoxTP(new CFWL_PictureBoxTP),
      m_pSrollBarTP(new CFWL_ScrollBarTP),
      m_pEditTP(new CFWL_EditTP),
      m_pComboBoxTP(new CFWL_ComboBoxTP),
      m_pMonthCalendarTP(new CFWL_MonthCalendarTP),
      m_pDateTimePickerTP(new CFWL_DateTimePickerTP),
      m_pPushButtonTP(new CFWL_PushButtonTP),
      m_pCaretTP(new CFWL_CaretTP),
      m_pBarcodeTP(new CFWL_BarcodeTP),
      m_pTextOut(new CFDE_TextOut),
      m_fCapacity(0.0f),
      m_dwCapacity(0),
      m_pCalendarFont(nullptr),
      m_pApp(pApp) {
  m_Rect.Reset();

  for (size_t i = 0; !m_pCalendarFont && i < FX_ArraySize(g_FWLTheme_CalFonts);
       ++i) {
    m_pCalendarFont = CFGAS_GEFont::LoadFont(g_FWLTheme_CalFonts[i], 0, 0,
                                             m_pApp->GetFDEFontMgr());
  }
  if (!m_pCalendarFont) {
#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
    m_pCalendarFont = m_pApp->GetFDEFontMgr()->GetDefFontByCodePage(
        FX_CODEPAGE_MSWin_WesternEuropean, 0, nullptr);
#else
    m_pCalendarFont = m_pApp->GetFDEFontMgr()->GetFontByCodePage(
        FX_CODEPAGE_MSWin_WesternEuropean, 0, nullptr);
#endif
  }

  ASSERT(m_pCalendarFont);
}

CXFA_FWLTheme::~CXFA_FWLTheme() {
  m_pTextOut.reset();
  if (m_pCalendarFont) {
    m_pCalendarFont->Release();
    m_pCalendarFont = nullptr;
  }
  FWLTHEME_Release();
}

void CXFA_FWLTheme::DrawBackground(CFWL_ThemeBackground* pParams) {
  GetTheme(pParams->m_pWidget)->DrawBackground(pParams);
}

void CXFA_FWLTheme::DrawText(CFWL_ThemeText* pParams) {
  if (pParams->m_wsText.IsEmpty())
    return;

  if (pParams->m_pWidget->GetClassID() == FWL_Type::MonthCalendar) {
    CXFA_FFWidget* pWidget = XFA_ThemeGetOuterWidget(pParams->m_pWidget);
    if (!pWidget)
      return;

    m_pTextOut->SetStyles(pParams->m_dwTTOStyles);
    m_pTextOut->SetAlignment(pParams->m_iTTOAlign);
    m_pTextOut->SetFont(m_pCalendarFont);
    m_pTextOut->SetFontSize(FWLTHEME_CAPACITY_FontSize);
    m_pTextOut->SetTextColor(FWLTHEME_CAPACITY_TextColor);
    if ((pParams->m_iPart == CFWL_Part::DatesIn) &&
        !(pParams->m_dwStates & FWL_ITEMSTATE_MCD_Flag) &&
        (pParams->m_dwStates &
         (CFWL_PartState_Hovered | CFWL_PartState_Selected))) {
      m_pTextOut->SetTextColor(0xFFFFFFFF);
    }
    if (pParams->m_iPart == CFWL_Part::Caption) {
      if (m_pMonthCalendarTP->GetThemeID(pParams->m_pWidget) == 0) {
        m_pTextOut->SetTextColor(ArgbEncode(0xff, 0, 153, 255));
      } else {
        m_pTextOut->SetTextColor(ArgbEncode(0xff, 128, 128, 0));
      }
    }
    CFX_Graphics* pGraphics = pParams->m_pGraphics;
    CFX_RenderDevice* pRenderDevice = pGraphics->GetRenderDevice();
    if (!pRenderDevice)
      return;

    m_pTextOut->SetRenderDevice(pRenderDevice);
    CFX_Matrix mtPart = pParams->m_matrix;
    CFX_Matrix* pMatrix = pGraphics->GetMatrix();
    if (pMatrix) {
      mtPart.Concat(*pMatrix);
    }
    m_pTextOut->SetMatrix(mtPart);
    m_pTextOut->DrawLogicText(pParams->m_wsText.c_str(),
                              pParams->m_wsText.GetLength(), pParams->m_rtPart);
    return;
  }
  CXFA_FFWidget* pWidget = XFA_ThemeGetOuterWidget(pParams->m_pWidget);
  if (!pWidget)
    return;

  CXFA_WidgetAcc* pAcc = pWidget->GetDataAcc();
  CFX_Graphics* pGraphics = pParams->m_pGraphics;
  CFX_RenderDevice* pRenderDevice = pGraphics->GetRenderDevice();
  if (!pRenderDevice)
    return;

  m_pTextOut->SetRenderDevice(pRenderDevice);
  m_pTextOut->SetStyles(pParams->m_dwTTOStyles);
  m_pTextOut->SetAlignment(pParams->m_iTTOAlign);
  m_pTextOut->SetFont(pAcc->GetFDEFont());
  m_pTextOut->SetFontSize(pAcc->GetFontSize());
  m_pTextOut->SetTextColor(pAcc->GetTextColor());
  CFX_Matrix mtPart = pParams->m_matrix;
  CFX_Matrix* pMatrix = pGraphics->GetMatrix();
  if (pMatrix)
    mtPart.Concat(*pMatrix);

  m_pTextOut->SetMatrix(mtPart);
  m_pTextOut->DrawLogicText(pParams->m_wsText.c_str(),
                            pParams->m_wsText.GetLength(), pParams->m_rtPart);
}

void* CXFA_FWLTheme::GetCapacity(CFWL_ThemePart* pThemePart,
                                 CFWL_WidgetCapacity dwCapacity) {
  switch (dwCapacity) {
    case CFWL_WidgetCapacity::Font: {
      if (CXFA_FFWidget* pWidget =
              XFA_ThemeGetOuterWidget(pThemePart->m_pWidget)) {
        return pWidget->GetDataAcc()->GetFDEFont();
      }
      break;
    }
    case CFWL_WidgetCapacity::FontSize: {
      if (CXFA_FFWidget* pWidget =
              XFA_ThemeGetOuterWidget(pThemePart->m_pWidget)) {
        m_fCapacity = pWidget->GetDataAcc()->GetFontSize();
        return &m_fCapacity;
      }
      break;
    }
    case CFWL_WidgetCapacity::TextColor: {
      if (CXFA_FFWidget* pWidget =
              XFA_ThemeGetOuterWidget(pThemePart->m_pWidget)) {
        m_dwCapacity = pWidget->GetDataAcc()->GetTextColor();
        return &m_dwCapacity;
      }
      break;
    }
    case CFWL_WidgetCapacity::LineHeight: {
      if (CXFA_FFWidget* pWidget =
              XFA_ThemeGetOuterWidget(pThemePart->m_pWidget)) {
        m_fCapacity = pWidget->GetDataAcc()->GetLineHeight();
        return &m_fCapacity;
      }
      break;
    }
    case CFWL_WidgetCapacity::ScrollBarWidth: {
      m_fCapacity = 9;
      return &m_fCapacity;
    }
    case CFWL_WidgetCapacity::UIMargin: {
      CXFA_FFWidget* pWidget = XFA_ThemeGetOuterWidget(pThemePart->m_pWidget);
      if (pWidget) {
        CXFA_LayoutItem* pItem = pWidget;
        CXFA_WidgetAcc* pWidgetAcc = pWidget->GetDataAcc();
        pWidgetAcc->GetUIMargin(m_Rect);
        if (CXFA_Para para = pWidgetAcc->GetPara()) {
          m_Rect.left += para.GetMarginLeft();
          if (pWidgetAcc->IsMultiLine()) {
            m_Rect.width += para.GetMarginRight();
          }
        }
        if (!pItem->GetPrev()) {
          if (pItem->GetNext()) {
            m_Rect.height = 0;
          }
        } else if (!pItem->GetNext()) {
          m_Rect.top = 0;
        } else {
          m_Rect.top = 0;
          m_Rect.height = 0;
        }
      }
      return &m_Rect;
    }
    case CFWL_WidgetCapacity::SpaceAboveBelow: {
      CXFA_FFWidget* pWidget = XFA_ThemeGetOuterWidget(pThemePart->m_pWidget);
      if (pWidget) {
        CXFA_WidgetAcc* pWidgetAcc = pWidget->GetDataAcc();
        if (CXFA_Para para = pWidgetAcc->GetPara()) {
          m_SizeAboveBelow.x = para.GetSpaceAbove();
          m_SizeAboveBelow.y = para.GetSpaceBelow();
        }
      }
      return &m_SizeAboveBelow;
    }
    default:
      break;
  }

  int dwCapValue = static_cast<int>(dwCapacity);
  if (pThemePart->m_pWidget->GetClassID() == FWL_Type::MonthCalendar &&
      dwCapValue >= static_cast<int>(CFWL_WidgetCapacity::Today) &&
      dwCapValue <= static_cast<int>(CFWL_WidgetCapacity::December)) {
    if (CXFA_FFWidget* pWidget =
            XFA_ThemeGetOuterWidget(pThemePart->m_pWidget)) {
      IXFA_AppProvider* pAppProvider = pWidget->GetAppProvider();
      m_wsResource.clear();
      switch (dwCapacity) {
        case CFWL_WidgetCapacity::Sun:
          pAppProvider->LoadString(XFA_IDS_StringWeekDay_Sun, m_wsResource);
          break;
        case CFWL_WidgetCapacity::Mon:
          pAppProvider->LoadString(XFA_IDS_StringWeekDay_Mon, m_wsResource);
          break;
        case CFWL_WidgetCapacity::Tue:
          pAppProvider->LoadString(XFA_IDS_StringWeekDay_Tue, m_wsResource);
          break;
        case CFWL_WidgetCapacity::Wed:
          pAppProvider->LoadString(XFA_IDS_StringWeekDay_Wed, m_wsResource);
          break;
        case CFWL_WidgetCapacity::Thu:
          pAppProvider->LoadString(XFA_IDS_StringWeekDay_Thu, m_wsResource);
          break;
        case CFWL_WidgetCapacity::Fri:
          pAppProvider->LoadString(XFA_IDS_StringWeekDay_Fri, m_wsResource);
          break;
        case CFWL_WidgetCapacity::Sat:
          pAppProvider->LoadString(XFA_IDS_StringWeekDay_Sat, m_wsResource);
          break;
        case CFWL_WidgetCapacity::January:
          pAppProvider->LoadString(XFA_IDS_StringMonth_Jan, m_wsResource);
          break;
        case CFWL_WidgetCapacity::February:
          pAppProvider->LoadString(XFA_IDS_StringMonth_Feb, m_wsResource);
          break;
        case CFWL_WidgetCapacity::March:
          pAppProvider->LoadString(XFA_IDS_StringMonth_March, m_wsResource);
          break;
        case CFWL_WidgetCapacity::April:
          pAppProvider->LoadString(XFA_IDS_StringMonth_April, m_wsResource);
          break;
        case CFWL_WidgetCapacity::May:
          pAppProvider->LoadString(XFA_IDS_StringMonth_May, m_wsResource);
          break;
        case CFWL_WidgetCapacity::June:
          pAppProvider->LoadString(XFA_IDS_StringMonth_June, m_wsResource);
          break;
        case CFWL_WidgetCapacity::July:
          pAppProvider->LoadString(XFA_IDS_StringMonth_July, m_wsResource);
          break;
        case CFWL_WidgetCapacity::August:
          pAppProvider->LoadString(XFA_IDS_StringMonth_Aug, m_wsResource);
          break;
        case CFWL_WidgetCapacity::September:
          pAppProvider->LoadString(XFA_IDS_StringMonth_Sept, m_wsResource);
          break;
        case CFWL_WidgetCapacity::October:
          pAppProvider->LoadString(XFA_IDS_StringMonth_Oct, m_wsResource);
          break;
        case CFWL_WidgetCapacity::November:
          pAppProvider->LoadString(XFA_IDS_StringMonth_Nov, m_wsResource);
          break;
        case CFWL_WidgetCapacity::December:
          pAppProvider->LoadString(XFA_IDS_StringMonth_Dec, m_wsResource);
          break;
        case CFWL_WidgetCapacity::Today:
          pAppProvider->LoadString(XFA_IDS_String_Today, m_wsResource);
          break;
        default:
          break;
      }
      if (!m_wsResource.IsEmpty())
        return &m_wsResource;
    }
  }
  return GetTheme(pThemePart->m_pWidget)->GetCapacity(pThemePart, dwCapacity);
}

bool CXFA_FWLTheme::IsCustomizedLayout(IFWL_Widget* pWidget) {
  return GetTheme(pWidget)->IsCustomizedLayout(pWidget);
}

void CXFA_FWLTheme::CalcTextRect(CFWL_ThemeText* pParams, CFX_RectF& rect) {
  if (pParams->m_pWidget->GetClassID() == FWL_Type::MonthCalendar) {
    CXFA_FFWidget* pWidget = XFA_ThemeGetOuterWidget(pParams->m_pWidget);
    if (!pWidget || !pParams || !m_pTextOut)
      return;

    m_pTextOut->SetFont(m_pCalendarFont);
    m_pTextOut->SetFontSize(FWLTHEME_CAPACITY_FontSize);
    m_pTextOut->SetTextColor(FWLTHEME_CAPACITY_TextColor);
    m_pTextOut->SetAlignment(pParams->m_iTTOAlign);
    m_pTextOut->SetStyles(pParams->m_dwTTOStyles);
    m_pTextOut->CalcLogicSize(pParams->m_wsText.c_str(),
                              pParams->m_wsText.GetLength(), rect);
  }

  CXFA_FFWidget* pWidget = XFA_ThemeGetOuterWidget(pParams->m_pWidget);
  if (!pWidget)
    return;

  CXFA_WidgetAcc* pAcc = pWidget->GetDataAcc();
  m_pTextOut->SetFont(pAcc->GetFDEFont());
  m_pTextOut->SetFontSize(pAcc->GetFontSize());
  m_pTextOut->SetTextColor(pAcc->GetTextColor());
  if (!pParams)
    return;

  m_pTextOut->SetAlignment(pParams->m_iTTOAlign);
  m_pTextOut->SetStyles(pParams->m_dwTTOStyles);
  m_pTextOut->CalcLogicSize(pParams->m_wsText.c_str(),
                            pParams->m_wsText.GetLength(), rect);
}

CFWL_WidgetTP* CXFA_FWLTheme::GetTheme(IFWL_Widget* pWidget) {
  switch (pWidget->GetClassID()) {
    case FWL_Type::CheckBox:
      return m_pCheckBoxTP.get();
    case FWL_Type::ListBox:
      return m_pListBoxTP.get();
    case FWL_Type::PictureBox:
      return m_pPictureBoxTP.get();
    case FWL_Type::ScrollBar:
      return m_pSrollBarTP.get();
    case FWL_Type::Edit:
      return m_pEditTP.get();
    case FWL_Type::ComboBox:
      return m_pComboBoxTP.get();
    case FWL_Type::MonthCalendar:
      return m_pMonthCalendarTP.get();
    case FWL_Type::DateTimePicker:
      return m_pDateTimePickerTP.get();
    case FWL_Type::PushButton:
      return m_pPushButtonTP.get();
    case FWL_Type::Caret:
      return m_pCaretTP.get();
    case FWL_Type::Barcode:
      return m_pBarcodeTP.get();
    default:
      return nullptr;
  }
}
