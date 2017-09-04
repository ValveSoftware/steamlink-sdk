// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/ifwl_picturebox.h"

#include "third_party/base/ptr_util.h"
#include "xfa/fwl/core/cfwl_picturebox.h"
#include "xfa/fwl/core/fwl_noteimp.h"

IFWL_PictureBox::IFWL_PictureBox(
    const IFWL_App* app,
    std::unique_ptr<CFWL_WidgetProperties> properties)
    : IFWL_Widget(app, std::move(properties), nullptr) {
  m_rtClient.Reset();
  m_rtImage.Reset();
  m_matrix.SetIdentity();
}

IFWL_PictureBox::~IFWL_PictureBox() {}

FWL_Type IFWL_PictureBox::GetClassID() const {
  return FWL_Type::PictureBox;
}

void IFWL_PictureBox::GetWidgetRect(CFX_RectF& rect, bool bAutoSize) {
  if (!bAutoSize) {
    rect = m_pProperties->m_rtWidget;
    return;
  }

  rect.Set(0, 0, 0, 0);
  if (!m_pProperties->m_pDataProvider)
    return;
  IFWL_Widget::GetWidgetRect(rect, true);
}

void IFWL_PictureBox::Update() {
  if (IsLocked())
    return;
  if (!m_pProperties->m_pThemeProvider)
    m_pProperties->m_pThemeProvider = GetAvailableTheme();

  GetClientRect(m_rtClient);
}

void IFWL_PictureBox::DrawWidget(CFX_Graphics* pGraphics,
                                 const CFX_Matrix* pMatrix) {
  if (!pGraphics)
    return;
  if (!m_pProperties->m_pThemeProvider)
    return;

  IFWL_ThemeProvider* pTheme = GetAvailableTheme();
  if (HasBorder())
    DrawBorder(pGraphics, CFWL_Part::Border, pTheme, pMatrix);
  if (HasEdge())
    DrawEdge(pGraphics, CFWL_Part::Edge, pTheme, pMatrix);
}

void IFWL_PictureBox::OnDrawWidget(CFX_Graphics* pGraphics,
                                   const CFX_Matrix* pMatrix) {
  DrawWidget(pGraphics, pMatrix);
}
