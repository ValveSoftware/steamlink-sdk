// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_IFWL_DATETIMEEDIT_H_
#define XFA_FWL_CORE_IFWL_DATETIMEEDIT_H_

#include "xfa/fwl/core/cfwl_widgetproperties.h"
#include "xfa/fwl/core/fwl_error.h"
#include "xfa/fwl/core/ifwl_edit.h"
#include "xfa/fwl/core/ifwl_widget.h"

class IFWL_DateTimeEdit : public IFWL_Edit {
 public:
  IFWL_DateTimeEdit(const IFWL_App* app,
                    std::unique_ptr<CFWL_WidgetProperties> properties,
                    IFWL_Widget* pOuter);

  // IFWL_Edit.
  void OnProcessMessage(CFWL_Message* pMessage) override;

 private:
  void DisForm_OnProcessMessage(CFWL_Message* pMessage);
};

#endif  // XFA_FWL_CORE_IFWL_DATETIMEEDIT_H_
