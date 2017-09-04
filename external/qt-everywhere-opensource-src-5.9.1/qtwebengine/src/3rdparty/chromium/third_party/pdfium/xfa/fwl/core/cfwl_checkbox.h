// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_CFWL_CHECKBOX_H_
#define XFA_FWL_CORE_CFWL_CHECKBOX_H_

#include "xfa/fwl/core/cfwl_widget.h"
#include "xfa/fwl/core/ifwl_checkbox.h"

class CFWL_CheckBox : public CFWL_Widget, public IFWL_CheckBoxDP {
 public:
  explicit CFWL_CheckBox(const IFWL_App* pApp);
  ~CFWL_CheckBox() override;

  void Initialize();

  // IFWL_DataProvider
  void GetCaption(IFWL_Widget* pWidget, CFX_WideString& wsCaption) override;

  // IFWL_CheckBoxDP
  FX_FLOAT GetBoxSize(IFWL_Widget* pWidget) override;

  void SetBoxSize(FX_FLOAT fHeight);

 private:
  FX_FLOAT m_fBoxHeight;
};

#endif  // XFA_FWL_CORE_CFWL_CHECKBOX_H_
