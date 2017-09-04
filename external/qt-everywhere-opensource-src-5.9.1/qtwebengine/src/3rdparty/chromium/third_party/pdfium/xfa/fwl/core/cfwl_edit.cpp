// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/cfwl_edit.h"

#include <memory>
#include <vector>

#include "third_party/base/ptr_util.h"

namespace {

IFWL_Edit* ToEdit(IFWL_Widget* widget) {
  return static_cast<IFWL_Edit*>(widget);
}

}  // namespace

CFWL_Edit::CFWL_Edit(const IFWL_App* app) : CFWL_Widget(app) {}

CFWL_Edit::~CFWL_Edit() {}

void CFWL_Edit::Initialize() {
  ASSERT(!m_pIface);

  m_pIface = pdfium::MakeUnique<IFWL_Edit>(
      m_pApp, pdfium::MakeUnique<CFWL_WidgetProperties>(), nullptr);

  CFWL_Widget::Initialize();
}

void CFWL_Edit::SetText(const CFX_WideString& wsText) {
  if (GetWidget())
    ToEdit(GetWidget())->SetText(wsText);
}

void CFWL_Edit::GetText(CFX_WideString& wsText,
                        int32_t nStart,
                        int32_t nCount) const {
  if (GetWidget())
    ToEdit(GetWidget())->GetText(wsText, nStart, nCount);
}

int32_t CFWL_Edit::CountSelRanges() const {
  return GetWidget() ? ToEdit(GetWidget())->CountSelRanges() : 0;
}

int32_t CFWL_Edit::GetSelRange(int32_t nIndex, int32_t& nStart) const {
  return GetWidget() ? ToEdit(GetWidget())->GetSelRange(nIndex, nStart) : 0;
}

int32_t CFWL_Edit::GetLimit() const {
  return GetWidget() ? ToEdit(GetWidget())->GetLimit() : -1;
}

void CFWL_Edit::SetLimit(int32_t nLimit) {
  if (GetWidget())
    ToEdit(GetWidget())->SetLimit(nLimit);
}

void CFWL_Edit::SetAliasChar(FX_WCHAR wAlias) {
  if (GetWidget())
    ToEdit(GetWidget())->SetAliasChar(wAlias);
}

void CFWL_Edit::SetScrollOffset(FX_FLOAT fScrollOffset) {
  if (GetWidget())
    ToEdit(GetWidget())->SetScrollOffset(fScrollOffset);
}
