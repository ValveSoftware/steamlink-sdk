// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_IFWL_FORMPROXY_H_
#define XFA_FWL_CORE_IFWL_FORMPROXY_H_

#include "xfa/fwl/core/ifwl_form.h"

class CFWL_WidgetProperties;

class IFWL_FormProxy : public IFWL_Form {
 public:
  IFWL_FormProxy(const IFWL_App* app,
                 std::unique_ptr<CFWL_WidgetProperties> properties,
                 IFWL_Widget* pOuter);
  ~IFWL_FormProxy() override;

  // IFWL_Widget
  FWL_Type GetClassID() const override;
  bool IsInstance(const CFX_WideStringC& wsClass) const override;
  void Update() override;
  void DrawWidget(CFX_Graphics* pGraphics,
                  const CFX_Matrix* pMatrix = nullptr) override;
  void OnProcessMessage(CFWL_Message* pMessage) override;
};

#endif  // XFA_FWL_CORE_IFWL_FORMPROXY_H_
