// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/app/xfa_ffimageedit.h"

#include "xfa/fwl/core/cfwl_msgmouse.h"
#include "xfa/fwl/core/cfwl_picturebox.h"
#include "xfa/fwl/core/fwl_noteimp.h"
#include "xfa/fwl/core/ifwl_app.h"
#include "xfa/fxfa/app/xfa_fffield.h"
#include "xfa/fxfa/xfa_ffdoc.h"
#include "xfa/fxfa/xfa_ffdocview.h"
#include "xfa/fxfa/xfa_ffpageview.h"
#include "xfa/fxfa/xfa_ffwidget.h"

CXFA_FFImageEdit::CXFA_FFImageEdit(CXFA_FFPageView* pPageView,
                                   CXFA_WidgetAcc* pDataAcc)
    : CXFA_FFField(pPageView, pDataAcc), m_pOldDelegate(nullptr) {}
CXFA_FFImageEdit::~CXFA_FFImageEdit() {
  CXFA_FFImageEdit::UnloadWidget();
}
bool CXFA_FFImageEdit::LoadWidget() {
  CFWL_PictureBox* pPictureBox = new CFWL_PictureBox(GetFWLApp());
  pPictureBox->Initialize();
  m_pNormalWidget = pPictureBox;
  m_pNormalWidget->SetLayoutItem(this);

  IFWL_Widget* pWidget = m_pNormalWidget->GetWidget();
  CFWL_NoteDriver* pNoteDriver = pWidget->GetOwnerApp()->GetNoteDriver();
  pNoteDriver->RegisterEventTarget(pWidget, pWidget);

  m_pOldDelegate = pPictureBox->GetDelegate();
  pPictureBox->SetDelegate(this);

  CXFA_FFField::LoadWidget();
  if (m_pDataAcc->GetImageEditImage()) {
    return true;
  }
  UpdateFWLData();
  return true;
}
void CXFA_FFImageEdit::UnloadWidget() {
  m_pDataAcc->SetImageEditImage(nullptr);
  CXFA_FFField::UnloadWidget();
}
void CXFA_FFImageEdit::RenderWidget(CFX_Graphics* pGS,
                                    CFX_Matrix* pMatrix,
                                    uint32_t dwStatus) {
  if (!IsMatchVisibleStatus(dwStatus)) {
    return;
  }
  CFX_Matrix mtRotate;
  GetRotateMatrix(mtRotate);
  if (pMatrix) {
    mtRotate.Concat(*pMatrix);
  }
  CXFA_FFWidget::RenderWidget(pGS, &mtRotate, dwStatus);
  CXFA_Border borderUI = m_pDataAcc->GetUIBorder();
  DrawBorder(pGS, borderUI, m_rtUI, &mtRotate);
  RenderCaption(pGS, &mtRotate);
  if (CFX_DIBitmap* pDIBitmap = m_pDataAcc->GetImageEditImage()) {
    CFX_RectF rtImage;
    m_pNormalWidget->GetWidgetRect(rtImage);
    int32_t iHorzAlign = XFA_ATTRIBUTEENUM_Left;
    int32_t iVertAlign = XFA_ATTRIBUTEENUM_Top;
    if (CXFA_Para para = m_pDataAcc->GetPara()) {
      iHorzAlign = para.GetHorizontalAlign();
      iVertAlign = para.GetVerticalAlign();
    }
    int32_t iAspect = XFA_ATTRIBUTEENUM_Fit;
    if (CXFA_Value value = m_pDataAcc->GetFormValue()) {
      if (CXFA_Image imageObj = value.GetImage()) {
        iAspect = imageObj.GetAspect();
      }
    }
    int32_t iImageXDpi = 0;
    int32_t iImageYDpi = 0;
    m_pDataAcc->GetImageEditDpi(iImageXDpi, iImageYDpi);
    XFA_DrawImage(pGS, rtImage, &mtRotate, pDIBitmap, iAspect, iImageXDpi,
                  iImageYDpi, iHorzAlign, iVertAlign);
  }
}

bool CXFA_FFImageEdit::OnLButtonDown(uint32_t dwFlags,
                                     FX_FLOAT fx,
                                     FX_FLOAT fy) {
  if (m_pDataAcc->GetAccess() != XFA_ATTRIBUTEENUM_Open)
    return false;

  if (!PtInActiveRect(fx, fy))
    return false;

  SetButtonDown(true);
  CFWL_MsgMouse ms;
  ms.m_dwCmd = FWL_MouseCommand::LeftButtonDown;
  ms.m_dwFlags = dwFlags;
  ms.m_fx = fx;
  ms.m_fy = fy;
  ms.m_pDstTarget = m_pNormalWidget->GetWidget();
  FWLToClient(ms.m_fx, ms.m_fy);
  TranslateFWLMessage(&ms);
  return true;
}

void CXFA_FFImageEdit::SetFWLRect() {
  if (!m_pNormalWidget) {
    return;
  }
  CFX_RectF rtUIMargin;
  m_pDataAcc->GetUIMargin(rtUIMargin);
  CFX_RectF rtImage(m_rtUI);
  rtImage.Deflate(rtUIMargin.left, rtUIMargin.top, rtUIMargin.width,
                  rtUIMargin.height);
  m_pNormalWidget->SetWidgetRect(rtImage);
}
bool CXFA_FFImageEdit::CommitData() {
  return true;
}
bool CXFA_FFImageEdit::UpdateFWLData() {
  m_pDataAcc->SetImageEditImage(nullptr);
  m_pDataAcc->LoadImageEditImage();
  return true;
}

void CXFA_FFImageEdit::OnProcessMessage(CFWL_Message* pMessage) {
  m_pOldDelegate->OnProcessMessage(pMessage);
}

void CXFA_FFImageEdit::OnProcessEvent(CFWL_Event* pEvent) {
  CXFA_FFField::OnProcessEvent(pEvent);
  m_pOldDelegate->OnProcessEvent(pEvent);
}

void CXFA_FFImageEdit::OnDrawWidget(CFX_Graphics* pGraphics,
                                    const CFX_Matrix* pMatrix) {
  m_pOldDelegate->OnDrawWidget(pGraphics, pMatrix);
}
