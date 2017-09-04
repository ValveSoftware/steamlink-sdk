// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/ifwl_formproxy.h"

#include "third_party/base/ptr_util.h"
#include "xfa/fwl/core/fwl_noteimp.h"

IFWL_FormProxy::IFWL_FormProxy(
    const IFWL_App* app,
    std::unique_ptr<CFWL_WidgetProperties> properties,
    IFWL_Widget* pOuter)
    : IFWL_Form(app, std::move(properties), pOuter) {}

IFWL_FormProxy::~IFWL_FormProxy() {}

FWL_Type IFWL_FormProxy::GetClassID() const {
  return FWL_Type::FormProxy;
}

bool IFWL_FormProxy::IsInstance(const CFX_WideStringC& wsClass) const {
  if (wsClass == CFX_WideStringC(FWL_CLASS_FormProxy))
    return true;
  return IFWL_Form::IsInstance(wsClass);
}

void IFWL_FormProxy::Update() {}

void IFWL_FormProxy::DrawWidget(CFX_Graphics* pGraphics,
                                const CFX_Matrix* pMatrix) {}

void IFWL_FormProxy::OnProcessMessage(CFWL_Message* pMessage) {
  m_pOuter->GetDelegate()->OnProcessMessage(pMessage);
}
