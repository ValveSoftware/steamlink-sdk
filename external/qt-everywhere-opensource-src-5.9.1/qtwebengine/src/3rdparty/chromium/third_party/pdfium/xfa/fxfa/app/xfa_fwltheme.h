// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_APP_XFA_FWLTHEME_H_
#define XFA_FXFA_APP_XFA_FWLTHEME_H_

#include <memory>

#include "xfa/fwl/core/ifwl_themeprovider.h"
#include "xfa/fwl/theme/cfwl_barcodetp.h"
#include "xfa/fwl/theme/cfwl_carettp.h"
#include "xfa/fwl/theme/cfwl_checkboxtp.h"
#include "xfa/fwl/theme/cfwl_comboboxtp.h"
#include "xfa/fwl/theme/cfwl_datetimepickertp.h"
#include "xfa/fwl/theme/cfwl_edittp.h"
#include "xfa/fwl/theme/cfwl_listboxtp.h"
#include "xfa/fwl/theme/cfwl_monthcalendartp.h"
#include "xfa/fwl/theme/cfwl_pictureboxtp.h"
#include "xfa/fwl/theme/cfwl_pushbuttontp.h"
#include "xfa/fwl/theme/cfwl_scrollbartp.h"
#include "xfa/fwl/theme/cfwl_widgettp.h"
#include "xfa/fxfa/xfa_ffapp.h"

class CXFA_FWLTheme final : public IFWL_ThemeProvider {
 public:
  CXFA_FWLTheme(CXFA_FFApp* pApp);
  ~CXFA_FWLTheme() override;

  // IFWL_ThemeProvider:
  void DrawBackground(CFWL_ThemeBackground* pParams) override;
  void DrawText(CFWL_ThemeText* pParams) override;
  void* GetCapacity(CFWL_ThemePart* pThemePart,
                    CFWL_WidgetCapacity dwCapacity) override;
  bool IsCustomizedLayout(IFWL_Widget* pWidget) override;
  void CalcTextRect(CFWL_ThemeText* pParams, CFX_RectF& rect) override;

 private:
  CFWL_WidgetTP* GetTheme(IFWL_Widget* pWidget);
  std::unique_ptr<CFWL_CheckBoxTP> m_pCheckBoxTP;
  std::unique_ptr<CFWL_ListBoxTP> m_pListBoxTP;
  std::unique_ptr<CFWL_PictureBoxTP> m_pPictureBoxTP;
  std::unique_ptr<CFWL_ScrollBarTP> m_pSrollBarTP;
  std::unique_ptr<CFWL_EditTP> m_pEditTP;
  std::unique_ptr<CFWL_ComboBoxTP> m_pComboBoxTP;
  std::unique_ptr<CFWL_MonthCalendarTP> m_pMonthCalendarTP;
  std::unique_ptr<CFWL_DateTimePickerTP> m_pDateTimePickerTP;
  std::unique_ptr<CFWL_PushButtonTP> m_pPushButtonTP;
  std::unique_ptr<CFWL_CaretTP> m_pCaretTP;
  std::unique_ptr<CFWL_BarcodeTP> m_pBarcodeTP;
  std::unique_ptr<CFDE_TextOut> m_pTextOut;
  FX_FLOAT m_fCapacity;
  uint32_t m_dwCapacity;
  CFGAS_GEFont* m_pCalendarFont;
  CFX_WideString m_wsResource;
  CXFA_FFApp* const m_pApp;
  CFX_RectF m_Rect;
  CFX_SizeF m_SizeAboveBelow;
};

CXFA_FFWidget* XFA_ThemeGetOuterWidget(IFWL_Widget* pWidget);

#endif  // XFA_FXFA_APP_XFA_FWLTHEME_H_
