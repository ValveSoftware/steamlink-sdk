// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/cfwl_widget.h"

#include "xfa/fde/tto/fde_textout.h"
#include "xfa/fwl/core/cfwl_themetext.h"
#include "xfa/fwl/core/cfwl_widgetmgr.h"
#include "xfa/fwl/core/fwl_noteimp.h"
#include "xfa/fwl/core/fwl_noteimp.h"
#include "xfa/fwl/core/ifwl_app.h"
#include "xfa/fwl/core/ifwl_themeprovider.h"

#define FWL_WGT_CalcHeight 2048
#define FWL_WGT_CalcWidth 2048
#define FWL_WGT_CalcMultiLineDefWidth 120.0f

CFWL_Widget::CFWL_Widget(const IFWL_App* app) : m_pApp(app) {}

CFWL_Widget::~CFWL_Widget() {}

void CFWL_Widget::Initialize() {
  ASSERT(m_pIface);
  m_pIface->SetAssociateWidget(this);
}

void CFWL_Widget::GetWidgetRect(CFX_RectF& rect, bool bAutoSize) {
  if (m_pIface)
    m_pIface->GetWidgetRect(rect, bAutoSize);
}

void CFWL_Widget::SetWidgetRect(const CFX_RectF& rect) {
  if (m_pIface)
    m_pIface->SetWidgetRect(rect);
}

void CFWL_Widget::ModifyStyles(uint32_t dwStylesAdded,
                               uint32_t dwStylesRemoved) {
  if (m_pIface)
    m_pIface->ModifyStyles(dwStylesAdded, dwStylesRemoved);
}

uint32_t CFWL_Widget::GetStylesEx() {
  return m_pIface ? m_pIface->GetStylesEx() : 0;
}

void CFWL_Widget::ModifyStylesEx(uint32_t dwStylesExAdded,
                                 uint32_t dwStylesExRemoved) {
  m_pIface->ModifyStylesEx(dwStylesExAdded, dwStylesExRemoved);
}

uint32_t CFWL_Widget::GetStates() {
  return m_pIface ? m_pIface->GetStates() : 0;
}

void CFWL_Widget::SetStates(uint32_t dwStates, bool bSet) {
  if (m_pIface)
    m_pIface->SetStates(dwStates, bSet);
}

void CFWL_Widget::SetLayoutItem(void* pItem) {
  if (m_pIface)
    m_pIface->SetLayoutItem(pItem);
}

void CFWL_Widget::Update() {
  if (m_pIface)
    m_pIface->Update();
}

void CFWL_Widget::LockUpdate() {
  if (m_pIface)
    m_pIface->LockUpdate();
}

void CFWL_Widget::UnlockUpdate() {
  if (m_pIface)
    m_pIface->UnlockUpdate();
}

FWL_WidgetHit CFWL_Widget::HitTest(FX_FLOAT fx, FX_FLOAT fy) {
  if (!m_pIface)
    return FWL_WidgetHit::Unknown;
  return m_pIface->HitTest(fx, fy);
}

void CFWL_Widget::DrawWidget(CFX_Graphics* pGraphics,
                             const CFX_Matrix* pMatrix) {
  if (m_pIface)
    m_pIface->DrawWidget(pGraphics, pMatrix);
}

IFWL_WidgetDelegate* CFWL_Widget::GetDelegate() const {
  return m_pIface ? m_pIface->GetDelegate() : nullptr;
}

void CFWL_Widget::SetDelegate(IFWL_WidgetDelegate* pDelegate) {
  if (m_pIface)
    m_pIface->SetDelegate(pDelegate);
}
