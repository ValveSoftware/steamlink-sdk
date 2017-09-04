// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_APP_XFA_FFTEXTEDIT_H_
#define XFA_FXFA_APP_XFA_FFTEXTEDIT_H_

#include <vector>

#include "xfa/fxfa/app/xfa_fffield.h"

class CXFA_FFTextEdit : public CXFA_FFField {
 public:
  CXFA_FFTextEdit(CXFA_FFPageView* pPageView, CXFA_WidgetAcc* pDataAcc);
  ~CXFA_FFTextEdit() override;

  // CXFA_FFField
  bool LoadWidget() override;
  void UpdateWidgetProperty() override;
  bool OnLButtonDown(uint32_t dwFlags, FX_FLOAT fx, FX_FLOAT fy) override;
  bool OnRButtonDown(uint32_t dwFlags, FX_FLOAT fx, FX_FLOAT fy) override;
  bool OnRButtonUp(uint32_t dwFlags, FX_FLOAT fx, FX_FLOAT fy) override;
  bool OnSetFocus(CXFA_FFWidget* pOldWidget) override;
  bool OnKillFocus(CXFA_FFWidget* pNewWidget) override;
  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnProcessEvent(CFWL_Event* pEvent) override;
  void OnDrawWidget(CFX_Graphics* pGraphics,
                    const CFX_Matrix* pMatrix = nullptr) override;

  void OnTextChanged(IFWL_Widget* pWidget,
                     const CFX_WideString& wsChanged,
                     const CFX_WideString& wsPrevText);
  void OnTextFull(IFWL_Widget* pWidget);
  bool CheckWord(const CFX_ByteStringC& sWord);

 protected:
  bool CommitData() override;
  bool UpdateFWLData() override;
  bool IsDataChanged() override;

  uint32_t GetAlignment();
  void ValidateNumberField(const CFX_WideString& wsText);

  IFWL_WidgetDelegate* m_pOldDelegate;
};

class CXFA_FFNumericEdit : public CXFA_FFTextEdit {
 public:
  CXFA_FFNumericEdit(CXFA_FFPageView* pPageView, CXFA_WidgetAcc* pDataAcc);
  ~CXFA_FFNumericEdit() override;

  // CXFA_FFTextEdit
  bool LoadWidget() override;
  void UpdateWidgetProperty() override;
  void OnProcessEvent(CFWL_Event* pEvent) override;

 public:
  bool OnValidate(IFWL_Widget* pWidget, CFX_WideString& wsText);
};

class CXFA_FFPasswordEdit : public CXFA_FFTextEdit {
 public:
  CXFA_FFPasswordEdit(CXFA_FFPageView* pPageView, CXFA_WidgetAcc* pDataAcc);
  ~CXFA_FFPasswordEdit() override;

  // CXFA_FFTextEdit
  bool LoadWidget() override;
  void UpdateWidgetProperty() override;

 protected:
};

enum XFA_DATETIMETYPE {
  XFA_DATETIMETYPE_Date = 0,
  XFA_DATETIMETYPE_Time,
  XFA_DATETIMETYPE_DateAndTime
};

class CXFA_FFDateTimeEdit : public CXFA_FFTextEdit {
 public:
  CXFA_FFDateTimeEdit(CXFA_FFPageView* pPageView, CXFA_WidgetAcc* pDataAcc);
  ~CXFA_FFDateTimeEdit() override;

  // CXFA_FFTextEdit
  bool GetBBox(CFX_RectF& rtBox,
               uint32_t dwStatus,
               bool bDrawFocus = false) override;
  bool LoadWidget() override;
  void UpdateWidgetProperty() override;
  void OnProcessEvent(CFWL_Event* pEvent) override;

  void OnSelectChanged(IFWL_Widget* pWidget,
                       int32_t iYear,
                       int32_t iMonth,
                       int32_t iDay);

 protected:
  bool PtInActiveRect(FX_FLOAT fx, FX_FLOAT fy) override;
  bool CommitData() override;
  bool UpdateFWLData() override;
  bool IsDataChanged() override;

  uint32_t GetAlignment();
};

#endif  // XFA_FXFA_APP_XFA_FFTEXTEDIT_H_
