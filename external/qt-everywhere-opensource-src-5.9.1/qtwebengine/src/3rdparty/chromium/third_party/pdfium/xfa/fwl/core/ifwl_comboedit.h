// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_IFWL_COMBOEDIT_H_
#define XFA_FWL_CORE_IFWL_COMBOEDIT_H_

#include "xfa/fwl/core/cfwl_widgetproperties.h"
#include "xfa/fwl/core/ifwl_edit.h"
#include "xfa/fwl/core/ifwl_widget.h"

class IFWL_ComboBox;

class IFWL_ComboEdit : public IFWL_Edit {
 public:
  IFWL_ComboEdit(const IFWL_App* app,
                 std::unique_ptr<CFWL_WidgetProperties> properties,
                 IFWL_Widget* pOuter);

  // IFWL_Edit.
  void OnProcessMessage(CFWL_Message* pMessage) override;

  void ClearSelected();
  void SetSelected();
  void FlagFocus(bool bSet);

 private:
  IFWL_ComboBox* m_pOuter;
};

#endif  // XFA_FWL_CORE_IFWL_COMBOEDIT_H_
