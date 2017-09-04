// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/ifwl_widget.h"

#include <algorithm>

#include "xfa/fde/tto/fde_textout.h"
#include "xfa/fwl/core/cfwl_evtkey.h"
#include "xfa/fwl/core/cfwl_evtkillfocus.h"
#include "xfa/fwl/core/cfwl_evtmouse.h"
#include "xfa/fwl/core/cfwl_evtmousewheel.h"
#include "xfa/fwl/core/cfwl_evtsetfocus.h"
#include "xfa/fwl/core/cfwl_evtsizechanged.h"
#include "xfa/fwl/core/cfwl_msgkey.h"
#include "xfa/fwl/core/cfwl_msgkillfocus.h"
#include "xfa/fwl/core/cfwl_msgmouse.h"
#include "xfa/fwl/core/cfwl_msgmousewheel.h"
#include "xfa/fwl/core/cfwl_msgsetfocus.h"
#include "xfa/fwl/core/cfwl_themebackground.h"
#include "xfa/fwl/core/cfwl_themepart.h"
#include "xfa/fwl/core/cfwl_themetext.h"
#include "xfa/fwl/core/cfwl_widgetmgr.h"
#include "xfa/fwl/core/fwl_noteimp.h"
#include "xfa/fwl/core/ifwl_app.h"
#include "xfa/fwl/core/ifwl_combobox.h"
#include "xfa/fwl/core/ifwl_form.h"
#include "xfa/fwl/core/ifwl_themeprovider.h"
#include "xfa/fwl/core/ifwl_widget.h"
#include "xfa/fxfa/xfa_ffapp.h"

#define FWL_STYLEEXT_MNU_Vert (1L << 0)
#define FWL_WGT_CalcHeight 2048
#define FWL_WGT_CalcWidth 2048
#define FWL_WGT_CalcMultiLineDefWidth 120.0f

IFWL_Widget::IFWL_Widget(const IFWL_App* app,
                         std::unique_ptr<CFWL_WidgetProperties> properties,
                         IFWL_Widget* pOuter)
    : m_pOwnerApp(app),
      m_pWidgetMgr(app->GetWidgetMgr()),
      m_pProperties(std::move(properties)),
      m_pOuter(pOuter),
      m_iLock(0),
      m_pLayoutItem(nullptr),
      m_pAssociate(nullptr),
      m_nEventKey(0),
      m_pDelegate(nullptr) {
  ASSERT(m_pWidgetMgr);

  IFWL_Widget* pParent = m_pProperties->m_pParent;
  m_pWidgetMgr->InsertWidget(pParent, this);
  if (IsChild())
    return;

  IFWL_Widget* pOwner = m_pProperties->m_pOwner;
  if (pOwner)
    m_pWidgetMgr->SetOwner(pOwner, this);
}

IFWL_Widget::~IFWL_Widget() {
  NotifyDriver();
  m_pWidgetMgr->RemoveWidget(this);
}

bool IFWL_Widget::IsInstance(const CFX_WideStringC& wsClass) const {
  return false;
}

void IFWL_Widget::GetWidgetRect(CFX_RectF& rect, bool bAutoSize) {
  if (!bAutoSize) {
    rect = m_pProperties->m_rtWidget;
    return;
  }

  if (HasEdge()) {
    FX_FLOAT fEdge = GetEdgeWidth();
    rect.Inflate(fEdge, fEdge);
  }
  if (HasBorder()) {
    FX_FLOAT fBorder = GetBorderSize();
    rect.Inflate(fBorder, fBorder);
  }
}

void IFWL_Widget::SetWidgetRect(const CFX_RectF& rect) {
  CFX_RectF rtOld = m_pProperties->m_rtWidget;
  m_pProperties->m_rtWidget = rect;
  if (IsChild()) {
    if (FXSYS_fabs(rtOld.width - rect.width) > 0.5f ||
        FXSYS_fabs(rtOld.height - rect.height) > 0.5f) {
      CFWL_EvtSizeChanged ev;
      ev.m_pSrcTarget = this;
      ev.m_rtOld = rtOld;
      ev.m_rtNew = rect;

      if (IFWL_WidgetDelegate* pDelegate = GetDelegate())
        pDelegate->OnProcessEvent(&ev);
    }
    return;
  }
  m_pWidgetMgr->SetWidgetRect_Native(this, rect);
}

void IFWL_Widget::GetClientRect(CFX_RectF& rect) {
  GetEdgeRect(rect);
  if (HasEdge()) {
    FX_FLOAT fEdge = GetEdgeWidth();
    rect.Deflate(fEdge, fEdge);
  }
}

void IFWL_Widget::SetParent(IFWL_Widget* pParent) {
  m_pProperties->m_pParent = pParent;
  m_pWidgetMgr->SetParent(pParent, this);
}

void IFWL_Widget::ModifyStyles(uint32_t dwStylesAdded,
                               uint32_t dwStylesRemoved) {
  m_pProperties->m_dwStyles =
      (m_pProperties->m_dwStyles & ~dwStylesRemoved) | dwStylesAdded;
}

void IFWL_Widget::ModifyStylesEx(uint32_t dwStylesExAdded,
                                 uint32_t dwStylesExRemoved) {
  m_pProperties->m_dwStyleExes =
      (m_pProperties->m_dwStyleExes & ~dwStylesExRemoved) | dwStylesExAdded;
}

static void NotifyHideChildWidget(CFWL_WidgetMgr* widgetMgr,
                                  IFWL_Widget* widget,
                                  CFWL_NoteDriver* noteDriver) {
  IFWL_Widget* child = widgetMgr->GetFirstChildWidget(widget);
  while (child) {
    noteDriver->NotifyTargetHide(child);
    NotifyHideChildWidget(widgetMgr, child, noteDriver);
    child = widgetMgr->GetNextSiblingWidget(child);
  }
}

void IFWL_Widget::SetStates(uint32_t dwStates, bool bSet) {
  bSet ? (m_pProperties->m_dwStates |= dwStates)
       : (m_pProperties->m_dwStates &= ~dwStates);
  if (!(dwStates & FWL_WGTSTATE_Invisible) || !bSet)
    return;

  CFWL_NoteDriver* noteDriver =
      static_cast<CFWL_NoteDriver*>(GetOwnerApp()->GetNoteDriver());
  CFWL_WidgetMgr* widgetMgr = GetOwnerApp()->GetWidgetMgr();
  noteDriver->NotifyTargetHide(this);
  IFWL_Widget* child = widgetMgr->GetFirstChildWidget(this);
  while (child) {
    noteDriver->NotifyTargetHide(child);
    NotifyHideChildWidget(widgetMgr, child, noteDriver);
    child = widgetMgr->GetNextSiblingWidget(child);
  }
  return;
}

void IFWL_Widget::Update() {}

FWL_WidgetHit IFWL_Widget::HitTest(FX_FLOAT fx, FX_FLOAT fy) {
  CFX_RectF rtClient;
  GetClientRect(rtClient);
  if (rtClient.Contains(fx, fy))
    return FWL_WidgetHit::Client;
  if (HasEdge()) {
    CFX_RectF rtEdge;
    GetEdgeRect(rtEdge);
    if (rtEdge.Contains(fx, fy))
      return FWL_WidgetHit::Edge;
  }
  if (HasBorder()) {
    CFX_RectF rtRelative;
    GetRelativeRect(rtRelative);
    if (rtRelative.Contains(fx, fy))
      return FWL_WidgetHit::Border;
  }
  return FWL_WidgetHit::Unknown;
}

void IFWL_Widget::TransformTo(IFWL_Widget* pWidget,
                              FX_FLOAT& fx,
                              FX_FLOAT& fy) {
  if (m_pWidgetMgr->IsFormDisabled()) {
    CFX_SizeF szOffset;
    if (IsParent(pWidget)) {
      szOffset = GetOffsetFromParent(pWidget);
    } else {
      szOffset = pWidget->GetOffsetFromParent(this);
      szOffset.x = -szOffset.x;
      szOffset.y = -szOffset.y;
    }
    fx += szOffset.x;
    fy += szOffset.y;
    return;
  }
  CFX_RectF r;
  CFX_Matrix m;
  IFWL_Widget* parent = GetParent();
  if (parent) {
    GetWidgetRect(r);
    fx += r.left;
    fy += r.top;
    GetMatrix(m, true);
    m.TransformPoint(fx, fy);
  }
  IFWL_Widget* form1 = m_pWidgetMgr->GetSystemFormWidget(this);
  if (!form1)
    return;
  if (!pWidget) {
    form1->GetWidgetRect(r);
    fx += r.left;
    fy += r.top;
    return;
  }
  IFWL_Widget* form2 = m_pWidgetMgr->GetSystemFormWidget(pWidget);
  if (!form2)
    return;
  if (form1 != form2) {
    form1->GetWidgetRect(r);
    fx += r.left;
    fy += r.top;
    form2->GetWidgetRect(r);
    fx -= r.left;
    fy -= r.top;
  }
  parent = pWidget->GetParent();
  if (parent) {
    pWidget->GetMatrix(m, true);
    CFX_Matrix m1;
    m1.SetIdentity();
    m1.SetReverse(m);
    m1.TransformPoint(fx, fy);
    pWidget->GetWidgetRect(r);
    fx -= r.left;
    fy -= r.top;
  }
}

void IFWL_Widget::GetMatrix(CFX_Matrix& matrix, bool bGlobal) {
  if (!m_pProperties)
    return;
  if (!bGlobal) {
    matrix.SetIdentity();
    return;
  }

  IFWL_Widget* parent = GetParent();
  CFX_ArrayTemplate<IFWL_Widget*> parents;
  while (parent) {
    parents.Add(parent);
    parent = parent->GetParent();
  }
  matrix.SetIdentity();
  CFX_Matrix ctmOnParent;
  CFX_RectF rect;
  int32_t count = parents.GetSize();
  for (int32_t i = count - 2; i >= 0; i--) {
    parent = parents.GetAt(i);
    parent->GetMatrix(ctmOnParent, false);
    parent->GetWidgetRect(rect);
    matrix.Concat(ctmOnParent, true);
    matrix.Translate(rect.left, rect.top, true);
  }
  CFX_Matrix m;
  m.SetIdentity();
  matrix.Concat(m, true);
  parents.RemoveAll();
}

void IFWL_Widget::DrawWidget(CFX_Graphics* pGraphics,
                             const CFX_Matrix* pMatrix) {}

void IFWL_Widget::SetThemeProvider(IFWL_ThemeProvider* pThemeProvider) {
  m_pProperties->m_pThemeProvider = pThemeProvider;
}

void IFWL_Widget::GetEdgeRect(CFX_RectF& rtEdge) {
  rtEdge = m_pProperties->m_rtWidget;
  rtEdge.left = rtEdge.top = 0;
  if (HasBorder()) {
    FX_FLOAT fCX = GetBorderSize();
    FX_FLOAT fCY = GetBorderSize(false);
    rtEdge.Deflate(fCX, fCY);
  }
}

FX_FLOAT IFWL_Widget::GetBorderSize(bool bCX) {
  FX_FLOAT* pfBorder = static_cast<FX_FLOAT*>(GetThemeCapacity(
      bCX ? CFWL_WidgetCapacity::CXBorder : CFWL_WidgetCapacity::CYBorder));
  if (!pfBorder)
    return 0;
  return *pfBorder;
}

FX_FLOAT IFWL_Widget::GetEdgeWidth() {
  CFWL_WidgetCapacity dwCapacity = CFWL_WidgetCapacity::None;
  switch (m_pProperties->m_dwStyles & FWL_WGTSTYLE_EdgeMask) {
    case FWL_WGTSTYLE_EdgeFlat: {
      dwCapacity = CFWL_WidgetCapacity::EdgeFlat;
      break;
    }
    case FWL_WGTSTYLE_EdgeRaised: {
      dwCapacity = CFWL_WidgetCapacity::EdgeRaised;
      break;
    }
    case FWL_WGTSTYLE_EdgeSunken: {
      dwCapacity = CFWL_WidgetCapacity::EdgeSunken;
      break;
    }
  }
  if (dwCapacity != CFWL_WidgetCapacity::None) {
    FX_FLOAT* fRet = static_cast<FX_FLOAT*>(GetThemeCapacity(dwCapacity));
    return fRet ? *fRet : 0;
  }
  return 0;
}

void IFWL_Widget::GetRelativeRect(CFX_RectF& rect) {
  rect = m_pProperties->m_rtWidget;
  rect.left = rect.top = 0;
}

void* IFWL_Widget::GetThemeCapacity(CFWL_WidgetCapacity dwCapacity) {
  IFWL_ThemeProvider* pTheme = GetAvailableTheme();
  if (!pTheme)
    return nullptr;

  CFWL_ThemePart part;
  part.m_pWidget = this;
  return pTheme->GetCapacity(&part, dwCapacity);
}

IFWL_ThemeProvider* IFWL_Widget::GetAvailableTheme() {
  if (m_pProperties->m_pThemeProvider)
    return m_pProperties->m_pThemeProvider;

  IFWL_Widget* pUp = this;
  do {
    pUp = (pUp->GetStyles() & FWL_WGTSTYLE_Popup)
              ? m_pWidgetMgr->GetOwnerWidget(pUp)
              : m_pWidgetMgr->GetParentWidget(pUp);
    if (pUp) {
      IFWL_ThemeProvider* pRet = pUp->GetThemeProvider();
      if (pRet)
        return pRet;
    }
  } while (pUp);
  return nullptr;
}

IFWL_Widget* IFWL_Widget::GetRootOuter() {
  IFWL_Widget* pRet = m_pOuter;
  if (!pRet)
    return nullptr;

  while (IFWL_Widget* pOuter = pRet->GetOuter())
    pRet = pOuter;
  return pRet;
}

CFX_SizeF IFWL_Widget::CalcTextSize(const CFX_WideString& wsText,
                                    IFWL_ThemeProvider* pTheme,
                                    bool bMultiLine,
                                    int32_t iLineWidth) {
  if (!pTheme)
    return CFX_SizeF();

  CFWL_ThemeText calPart;
  calPart.m_pWidget = this;
  calPart.m_wsText = wsText;
  calPart.m_dwTTOStyles =
      bMultiLine ? FDE_TTOSTYLE_LineWrap : FDE_TTOSTYLE_SingleLine;
  calPart.m_iTTOAlign = FDE_TTOALIGNMENT_TopLeft;
  CFX_RectF rect;
  FX_FLOAT fWidth = bMultiLine
                        ? (iLineWidth > 0 ? (FX_FLOAT)iLineWidth
                                          : FWL_WGT_CalcMultiLineDefWidth)
                        : FWL_WGT_CalcWidth;
  rect.Set(0, 0, fWidth, FWL_WGT_CalcHeight);
  pTheme->CalcTextRect(&calPart, rect);
  return CFX_SizeF(rect.width, rect.height);
}

void IFWL_Widget::CalcTextRect(const CFX_WideString& wsText,
                               IFWL_ThemeProvider* pTheme,
                               uint32_t dwTTOStyles,
                               int32_t iTTOAlign,
                               CFX_RectF& rect) {
  CFWL_ThemeText calPart;
  calPart.m_pWidget = this;
  calPart.m_wsText = wsText;
  calPart.m_dwTTOStyles = dwTTOStyles;
  calPart.m_iTTOAlign = iTTOAlign;
  pTheme->CalcTextRect(&calPart, rect);
}

void IFWL_Widget::SetFocus(bool bFocus) {
  if (m_pWidgetMgr->IsFormDisabled())
    return;

  const IFWL_App* pApp = GetOwnerApp();
  if (!pApp)
    return;

  CFWL_NoteDriver* pDriver =
      static_cast<CFWL_NoteDriver*>(pApp->GetNoteDriver());
  if (!pDriver)
    return;

  IFWL_Widget* curFocus = pDriver->GetFocus();
  if (bFocus && curFocus != this)
    pDriver->SetFocus(this);
  else if (!bFocus && curFocus == this)
    pDriver->SetFocus(nullptr);
}

void IFWL_Widget::SetGrab(bool bSet) {
  const IFWL_App* pApp = GetOwnerApp();
  if (!pApp)
    return;

  CFWL_NoteDriver* pDriver =
      static_cast<CFWL_NoteDriver*>(pApp->GetNoteDriver());
  pDriver->SetGrab(this, bSet);
}

void IFWL_Widget::GetPopupPos(FX_FLOAT fMinHeight,
                              FX_FLOAT fMaxHeight,
                              const CFX_RectF& rtAnchor,
                              CFX_RectF& rtPopup) {
  if (GetClassID() == FWL_Type::ComboBox) {
    if (m_pWidgetMgr->IsFormDisabled()) {
      m_pWidgetMgr->GetAdapterPopupPos(this, fMinHeight, fMaxHeight, rtAnchor,
                                       rtPopup);
      return;
    }
    GetPopupPosComboBox(fMinHeight, fMaxHeight, rtAnchor, rtPopup);
    return;
  }
  if (GetClassID() == FWL_Type::DateTimePicker &&
      m_pWidgetMgr->IsFormDisabled()) {
    m_pWidgetMgr->GetAdapterPopupPos(this, fMinHeight, fMaxHeight, rtAnchor,
                                     rtPopup);
    return;
  }
  GetPopupPosGeneral(fMinHeight, fMaxHeight, rtAnchor, rtPopup);
}

bool IFWL_Widget::GetPopupPosMenu(FX_FLOAT fMinHeight,
                                  FX_FLOAT fMaxHeight,
                                  const CFX_RectF& rtAnchor,
                                  CFX_RectF& rtPopup) {
  FX_FLOAT fx = 0;
  FX_FLOAT fy = 0;

  if (GetStylesEx() & FWL_STYLEEXT_MNU_Vert) {
    bool bLeft = m_pProperties->m_rtWidget.left < 0;
    FX_FLOAT fRight = rtAnchor.right() + rtPopup.width;
    TransformTo(nullptr, fx, fy);
    if (fRight + fx > 0.0f || bLeft) {
      rtPopup.Set(rtAnchor.left - rtPopup.width, rtAnchor.top, rtPopup.width,
                  rtPopup.height);
    } else {
      rtPopup.Set(rtAnchor.right(), rtAnchor.top, rtPopup.width,
                  rtPopup.height);
    }
  } else {
    FX_FLOAT fBottom = rtAnchor.bottom() + rtPopup.height;
    TransformTo(nullptr, fx, fy);
    if (fBottom + fy > 0.0f) {
      rtPopup.Set(rtAnchor.left, rtAnchor.top - rtPopup.height, rtPopup.width,
                  rtPopup.height);
    } else {
      rtPopup.Set(rtAnchor.left, rtAnchor.bottom(), rtPopup.width,
                  rtPopup.height);
    }
  }
  rtPopup.Offset(fx, fy);
  return true;
}

bool IFWL_Widget::GetPopupPosComboBox(FX_FLOAT fMinHeight,
                                      FX_FLOAT fMaxHeight,
                                      const CFX_RectF& rtAnchor,
                                      CFX_RectF& rtPopup) {
  FX_FLOAT fx = 0;
  FX_FLOAT fy = 0;

  FX_FLOAT fPopHeight = rtPopup.height;
  if (rtPopup.height > fMaxHeight)
    fPopHeight = fMaxHeight;
  else if (rtPopup.height < fMinHeight)
    fPopHeight = fMinHeight;

  FX_FLOAT fWidth = std::max(rtAnchor.width, rtPopup.width);
  FX_FLOAT fBottom = rtAnchor.bottom() + fPopHeight;
  TransformTo(nullptr, fx, fy);
  if (fBottom + fy > 0.0f)
    rtPopup.Set(rtAnchor.left, rtAnchor.top - fPopHeight, fWidth, fPopHeight);
  else
    rtPopup.Set(rtAnchor.left, rtAnchor.bottom(), fWidth, fPopHeight);

  rtPopup.Offset(fx, fy);
  return true;
}

bool IFWL_Widget::GetPopupPosGeneral(FX_FLOAT fMinHeight,
                                     FX_FLOAT fMaxHeight,
                                     const CFX_RectF& rtAnchor,
                                     CFX_RectF& rtPopup) {
  FX_FLOAT fx = 0;
  FX_FLOAT fy = 0;

  TransformTo(nullptr, fx, fy);
  if (rtAnchor.bottom() + fy > 0.0f) {
    rtPopup.Set(rtAnchor.left, rtAnchor.top - rtPopup.height, rtPopup.width,
                rtPopup.height);
  } else {
    rtPopup.Set(rtAnchor.left, rtAnchor.bottom(), rtPopup.width,
                rtPopup.height);
  }
  rtPopup.Offset(fx, fy);
  return true;
}

void IFWL_Widget::RegisterEventTarget(IFWL_Widget* pEventSource,
                                      uint32_t dwFilter) {
  const IFWL_App* pApp = GetOwnerApp();
  if (!pApp)
    return;

  CFWL_NoteDriver* pNoteDriver = pApp->GetNoteDriver();
  if (!pNoteDriver)
    return;

  pNoteDriver->RegisterEventTarget(this, pEventSource, dwFilter);
}

void IFWL_Widget::UnregisterEventTarget() {
  const IFWL_App* pApp = GetOwnerApp();
  if (!pApp)
    return;

  CFWL_NoteDriver* pNoteDriver = pApp->GetNoteDriver();
  if (!pNoteDriver)
    return;

  pNoteDriver->UnregisterEventTarget(this);
}

void IFWL_Widget::DispatchKeyEvent(CFWL_MsgKey* pNote) {
  if (!pNote)
    return;

  auto pEvent = pdfium::MakeUnique<CFWL_EvtKey>();
  pEvent->m_pSrcTarget = this;
  pEvent->m_dwCmd = pNote->m_dwCmd;
  pEvent->m_dwKeyCode = pNote->m_dwKeyCode;
  pEvent->m_dwFlags = pNote->m_dwFlags;
  DispatchEvent(pEvent.get());
}

void IFWL_Widget::DispatchEvent(CFWL_Event* pEvent) {
  if (m_pOuter) {
    m_pOuter->GetDelegate()->OnProcessEvent(pEvent);
    return;
  }
  const IFWL_App* pApp = GetOwnerApp();
  if (!pApp)
    return;

  CFWL_NoteDriver* pNoteDriver = pApp->GetNoteDriver();
  if (!pNoteDriver)
    return;
  pNoteDriver->SendEvent(pEvent);
}

void IFWL_Widget::Repaint(const CFX_RectF* pRect) {
  if (pRect) {
    m_pWidgetMgr->RepaintWidget(this, pRect);
    return;
  }
  CFX_RectF rect;
  rect = m_pProperties->m_rtWidget;
  rect.left = rect.top = 0;
  m_pWidgetMgr->RepaintWidget(this, &rect);
}

void IFWL_Widget::DrawBackground(CFX_Graphics* pGraphics,
                                 CFWL_Part iPartBk,
                                 IFWL_ThemeProvider* pTheme,
                                 const CFX_Matrix* pMatrix) {
  CFX_RectF rtRelative;
  GetRelativeRect(rtRelative);
  CFWL_ThemeBackground param;
  param.m_pWidget = this;
  param.m_iPart = iPartBk;
  param.m_pGraphics = pGraphics;
  if (pMatrix)
    param.m_matrix.Concat(*pMatrix, true);
  param.m_rtPart = rtRelative;
  pTheme->DrawBackground(&param);
}

void IFWL_Widget::DrawBorder(CFX_Graphics* pGraphics,
                             CFWL_Part iPartBorder,
                             IFWL_ThemeProvider* pTheme,
                             const CFX_Matrix* pMatrix) {
  CFX_RectF rtRelative;
  GetRelativeRect(rtRelative);
  CFWL_ThemeBackground param;
  param.m_pWidget = this;
  param.m_iPart = iPartBorder;
  param.m_pGraphics = pGraphics;
  if (pMatrix)
    param.m_matrix.Concat(*pMatrix, true);
  param.m_rtPart = rtRelative;
  pTheme->DrawBackground(&param);
}

void IFWL_Widget::DrawEdge(CFX_Graphics* pGraphics,
                           CFWL_Part iPartEdge,
                           IFWL_ThemeProvider* pTheme,
                           const CFX_Matrix* pMatrix) {
  CFX_RectF rtEdge;
  GetEdgeRect(rtEdge);
  CFWL_ThemeBackground param;
  param.m_pWidget = this;
  param.m_iPart = iPartEdge;
  param.m_pGraphics = pGraphics;
  if (pMatrix)
    param.m_matrix.Concat(*pMatrix, true);
  param.m_rtPart = rtEdge;
  pTheme->DrawBackground(&param);
}

void IFWL_Widget::NotifyDriver() {
  const IFWL_App* pApp = GetOwnerApp();
  if (!pApp)
    return;

  CFWL_NoteDriver* pDriver =
      static_cast<CFWL_NoteDriver*>(pApp->GetNoteDriver());
  if (!pDriver)
    return;

  pDriver->NotifyTargetDestroy(this);
}

CFX_SizeF IFWL_Widget::GetOffsetFromParent(IFWL_Widget* pParent) {
  if (pParent == this)
    return CFX_SizeF();

  CFWL_WidgetMgr* pWidgetMgr = GetOwnerApp()->GetWidgetMgr();
  if (!pWidgetMgr)
    return CFX_SizeF();

  CFX_SizeF szRet(m_pProperties->m_rtWidget.left,
                  m_pProperties->m_rtWidget.top);

  IFWL_Widget* pDstWidget = GetParent();
  while (pDstWidget && pDstWidget != pParent) {
    CFX_RectF rtDst;
    pDstWidget->GetWidgetRect(rtDst);
    szRet += CFX_SizeF(rtDst.left, rtDst.top);
    pDstWidget = pWidgetMgr->GetParentWidget(pDstWidget);
  }
  return szRet;
}

bool IFWL_Widget::IsParent(IFWL_Widget* pParent) {
  IFWL_Widget* pUpWidget = GetParent();
  while (pUpWidget) {
    if (pUpWidget == pParent)
      return true;
    pUpWidget = pUpWidget->GetParent();
  }
  return false;
}

void IFWL_Widget::OnProcessMessage(CFWL_Message* pMessage) {
  if (!pMessage->m_pDstTarget)
    return;

  IFWL_Widget* pWidget = pMessage->m_pDstTarget;
  CFWL_MessageType dwMsgCode = pMessage->GetClassID();
  switch (dwMsgCode) {
    case CFWL_MessageType::Mouse: {
      CFWL_MsgMouse* pMsgMouse = static_cast<CFWL_MsgMouse*>(pMessage);
      CFWL_EvtMouse evt;
      evt.m_pSrcTarget = pWidget;
      evt.m_pDstTarget = pWidget;
      evt.m_dwCmd = pMsgMouse->m_dwCmd;
      evt.m_dwFlags = pMsgMouse->m_dwFlags;
      evt.m_fx = pMsgMouse->m_fx;
      evt.m_fy = pMsgMouse->m_fy;
      pWidget->DispatchEvent(&evt);
      break;
    }
    case CFWL_MessageType::MouseWheel: {
      CFWL_MsgMouseWheel* pMsgMouseWheel =
          static_cast<CFWL_MsgMouseWheel*>(pMessage);
      CFWL_EvtMouseWheel evt;
      evt.m_pSrcTarget = pWidget;
      evt.m_pDstTarget = pWidget;
      evt.m_dwFlags = pMsgMouseWheel->m_dwFlags;
      evt.m_fDeltaX = pMsgMouseWheel->m_fDeltaX;
      evt.m_fDeltaY = pMsgMouseWheel->m_fDeltaY;
      evt.m_fx = pMsgMouseWheel->m_fx;
      evt.m_fy = pMsgMouseWheel->m_fy;
      pWidget->DispatchEvent(&evt);
      break;
    }
    case CFWL_MessageType::Key: {
      CFWL_MsgKey* pMsgKey = static_cast<CFWL_MsgKey*>(pMessage);
      CFWL_EvtKey evt;
      evt.m_pSrcTarget = pWidget;
      evt.m_pDstTarget = pWidget;
      evt.m_dwKeyCode = pMsgKey->m_dwKeyCode;
      evt.m_dwFlags = pMsgKey->m_dwFlags;
      evt.m_dwCmd = pMsgKey->m_dwCmd;
      pWidget->DispatchEvent(&evt);
      break;
    }
    case CFWL_MessageType::SetFocus: {
      CFWL_MsgSetFocus* pMsgSetFocus = static_cast<CFWL_MsgSetFocus*>(pMessage);
      CFWL_EvtSetFocus evt;
      evt.m_pSrcTarget = pMsgSetFocus->m_pDstTarget;
      evt.m_pDstTarget = pMsgSetFocus->m_pDstTarget;
      evt.m_pSetFocus = pWidget;
      pWidget->DispatchEvent(&evt);
      break;
    }
    case CFWL_MessageType::KillFocus: {
      CFWL_MsgKillFocus* pMsgKillFocus =
          static_cast<CFWL_MsgKillFocus*>(pMessage);
      CFWL_EvtKillFocus evt;
      evt.m_pSrcTarget = pMsgKillFocus->m_pDstTarget;
      evt.m_pDstTarget = pMsgKillFocus->m_pDstTarget;
      evt.m_pKillFocus = pWidget;
      pWidget->DispatchEvent(&evt);
      break;
    }
    default:
      break;
  }
}

void IFWL_Widget::OnProcessEvent(CFWL_Event* pEvent) {}

void IFWL_Widget::OnDrawWidget(CFX_Graphics* pGraphics,
                               const CFX_Matrix* pMatrix) {}
