// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/cfwl_checkbox.h"

#include <memory>

#include "third_party/base/ptr_util.h"
#include "xfa/fwl/core/fwl_error.h"

CFWL_CheckBox::CFWL_CheckBox(const IFWL_App* app)
    : CFWL_Widget(app), m_fBoxHeight(16.0f) {}

CFWL_CheckBox::~CFWL_CheckBox() {}

void CFWL_CheckBox::Initialize() {
  ASSERT(!m_pIface);

  m_pIface = pdfium::MakeUnique<IFWL_CheckBox>(
      m_pApp, pdfium::MakeUnique<CFWL_WidgetProperties>(this));

  CFWL_Widget::Initialize();
}

void CFWL_CheckBox::SetBoxSize(FX_FLOAT fHeight) {
  m_fBoxHeight = fHeight;
}

void CFWL_CheckBox::GetCaption(IFWL_Widget* pWidget,
                               CFX_WideString& wsCaption) {
  wsCaption = L"Check box";
}

FX_FLOAT CFWL_CheckBox::GetBoxSize(IFWL_Widget* pWidget) {
  return m_fBoxHeight;
}
