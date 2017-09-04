// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_CFWL_DATETIMEPICKER_H_
#define XFA_FWL_CORE_CFWL_DATETIMEPICKER_H_

#include "xfa/fwl/core/cfwl_widget.h"
#include "xfa/fwl/core/ifwl_datetimepicker.h"

class CFWL_DateTimePicker : public CFWL_Widget, public IFWL_DateTimePickerDP {
 public:
  explicit CFWL_DateTimePicker(const IFWL_App* pApp);
  ~CFWL_DateTimePicker() override;

  void Initialize();

  // IFWL_DataProvider
  void GetCaption(IFWL_Widget* pWidget, CFX_WideString& wsCaption) override;

  // IFWL_DateTimePickerDP
  void GetToday(IFWL_Widget* pWidget,
                int32_t& iYear,
                int32_t& iMonth,
                int32_t& iDay) override;

  void GetEditText(CFX_WideString& wsText);
  void SetEditText(const CFX_WideString& wsText);

  int32_t CountSelRanges();
  int32_t GetSelRange(int32_t nIndex, int32_t& nStart);

  void SetCurSel(int32_t iYear, int32_t iMonth, int32_t iDay);
  void GetBBox(CFX_RectF& rect);
  void SetEditLimit(int32_t nLimit);
  void ModifyEditStylesEx(uint32_t dwStylesExAdded, uint32_t dwStylesExRemoved);
};

#endif  // XFA_FWL_CORE_CFWL_DATETIMEPICKER_H_
