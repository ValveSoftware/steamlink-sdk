// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/ifwl_form.h"

#include "third_party/base/ptr_util.h"
#include "xfa/fde/tto/fde_textout.h"
#include "xfa/fwl/core/cfwl_evtclose.h"
#include "xfa/fwl/core/cfwl_msgmouse.h"
#include "xfa/fwl/core/cfwl_themebackground.h"
#include "xfa/fwl/core/cfwl_themepart.h"
#include "xfa/fwl/core/cfwl_themetext.h"
#include "xfa/fwl/core/cfwl_widgetmgr.h"
#include "xfa/fwl/core/fwl_noteimp.h"
#include "xfa/fwl/core/ifwl_app.h"
#include "xfa/fwl/core/ifwl_formproxy.h"
#include "xfa/fwl/core/ifwl_themeprovider.h"
#include "xfa/fwl/theme/cfwl_widgettp.h"

namespace {

const int kSystemButtonSize = 21;
const int kSystemButtonMargin = 5;
const int kSystemButtonSpan = 2;

}  // namespace

namespace {

const uint8_t kCornerEnlarge = 10;

}  // namespace

RestoreInfo::RestoreInfo() {}

RestoreInfo::~RestoreInfo() {}

IFWL_Form::IFWL_Form(const IFWL_App* app,
                     std::unique_ptr<CFWL_WidgetProperties> properties,
                     IFWL_Widget* pOuter)
    : IFWL_Widget(app, std::move(properties), pOuter),
#if (_FX_OS_ == _FX_MACOSX_)
      m_bMouseIn(false),
#endif
      m_pCloseBox(nullptr),
      m_pMinBox(nullptr),
      m_pMaxBox(nullptr),
      m_pCaptionBox(nullptr),
      m_pSubFocus(nullptr),
      m_fCXBorder(0),
      m_fCYBorder(0),
      m_iCaptureBtn(-1),
      m_iSysBox(0),
      m_eResizeType(FORM_RESIZETYPE_None),
      m_bLButtonDown(false),
      m_bMaximized(false),
      m_bSetMaximize(false),
      m_bCustomizeLayout(false),
      m_bDoModalFlag(false),
      m_pBigIcon(nullptr),
      m_pSmallIcon(nullptr) {
  m_rtRelative.Reset();
  m_rtCaption.Reset();
  m_rtRestore.Reset();
  m_rtCaptionText.Reset();
  m_rtIcon.Reset();

  RegisterForm();
  RegisterEventTarget();
}

IFWL_Form::~IFWL_Form() {
  UnregisterEventTarget();
  UnRegisterForm();
  RemoveSysButtons();
}

FWL_Type IFWL_Form::GetClassID() const {
  return FWL_Type::Form;
}

bool IFWL_Form::IsInstance(const CFX_WideStringC& wsClass) const {
  if (wsClass == CFX_WideStringC(FWL_CLASS_Form))
    return true;
  return IFWL_Widget::IsInstance(wsClass);
}

void IFWL_Form::GetWidgetRect(CFX_RectF& rect, bool bAutoSize) {
  if (!bAutoSize) {
    rect = m_pProperties->m_rtWidget;
    return;
  }

  rect.Reset();
  FX_FLOAT fCapHeight = GetCaptionHeight();
  FX_FLOAT fCXBorder = GetBorderSize(true);
  FX_FLOAT fCYBorder = GetBorderSize(false);
  FX_FLOAT fEdge = GetEdgeWidth();
  rect.height += fCapHeight + fCYBorder + fEdge + fEdge;
  rect.width += fCXBorder + fCXBorder + fEdge + fEdge;
}

void IFWL_Form::GetClientRect(CFX_RectF& rect) {
  if ((m_pProperties->m_dwStyles & FWL_WGTSTYLE_Caption) == 0) {
    rect = m_pProperties->m_rtWidget;
    rect.Offset(-rect.left, -rect.top);
    return;
  }

#ifdef FWL_UseMacSystemBorder
  rect = m_rtRelative;
  CFWL_WidgetMgr* pWidgetMgr = GetOwnerApp()->GetWidgetMgr();
  if (!pWidgetMgr)
    return;

  rect.left = 0;
  rect.top = 0;
#else
  FX_FLOAT x = 0;
  FX_FLOAT y = 0;
  FX_FLOAT t = 0;
  IFWL_ThemeProvider* pTheme = m_pProperties->m_pThemeProvider;
  if (pTheme) {
    CFWL_ThemePart part;
    part.m_pWidget = this;
    x = *static_cast<FX_FLOAT*>(
        pTheme->GetCapacity(&part, CFWL_WidgetCapacity::CXBorder));
    y = *static_cast<FX_FLOAT*>(
        pTheme->GetCapacity(&part, CFWL_WidgetCapacity::CYBorder));
    t = *static_cast<FX_FLOAT*>(
        pTheme->GetCapacity(&part, CFWL_WidgetCapacity::CYCaption));
  }
  rect = m_pProperties->m_rtWidget;
  rect.Offset(-rect.left, -rect.top);
  rect.Deflate(x, t, x, y);
#endif
}

void IFWL_Form::Update() {
  if (m_iLock > 0)
    return;
  if (!m_pProperties->m_pThemeProvider)
    m_pProperties->m_pThemeProvider = GetAvailableTheme();

#ifndef FWL_UseMacSystemBorder
  SetThemeData();
  if (m_pProperties->m_dwStyles & FWL_WGTSTYLE_Icon)
    UpdateIcon();
#endif

  UpdateCaption();
  Layout();
}

FWL_WidgetHit IFWL_Form::HitTest(FX_FLOAT fx, FX_FLOAT fy) {
  GetAvailableTheme();

  if (m_pCloseBox && m_pCloseBox->m_rtBtn.Contains(fx, fy))
    return FWL_WidgetHit::CloseBox;
  if (m_pMaxBox && m_pMaxBox->m_rtBtn.Contains(fx, fy))
    return FWL_WidgetHit::MaxBox;
  if (m_pMinBox && m_pMinBox->m_rtBtn.Contains(fx, fy))
    return FWL_WidgetHit::MinBox;

  CFX_RectF rtCap;
  rtCap.Set(m_rtCaption.left + m_fCYBorder, m_rtCaption.top + m_fCXBorder,
            m_rtCaption.width - kSystemButtonSize * m_iSysBox - 2 * m_fCYBorder,
            m_rtCaption.height - m_fCXBorder);
  if (rtCap.Contains(fx, fy))
    return FWL_WidgetHit::Titlebar;
  if ((m_pProperties->m_dwStyles & FWL_WGTSTYLE_Border) &&
      (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_FRM_Resize)) {
    FX_FLOAT fWidth = m_rtRelative.width - 2 * (m_fCYBorder + kCornerEnlarge);
    FX_FLOAT fHeight = m_rtRelative.height - 2 * (m_fCXBorder + kCornerEnlarge);

    CFX_RectF rt;
    rt.Set(0, m_fCXBorder + kCornerEnlarge, m_fCYBorder, fHeight);
    if (rt.Contains(fx, fy))
      return FWL_WidgetHit::Left;

    rt.Set(m_rtRelative.width - m_fCYBorder, m_fCXBorder + kCornerEnlarge,
           m_fCYBorder, fHeight);
    if (rt.Contains(fx, fy))
      return FWL_WidgetHit::Right;

    rt.Set(m_fCYBorder + kCornerEnlarge, 0, fWidth, m_fCXBorder);
    if (rt.Contains(fx, fy))
      return FWL_WidgetHit::Top;

    rt.Set(m_fCYBorder + kCornerEnlarge, m_rtRelative.height - m_fCXBorder,
           fWidth, m_fCXBorder);
    if (rt.Contains(fx, fy))
      return FWL_WidgetHit::Bottom;

    rt.Set(0, 0, m_fCYBorder + kCornerEnlarge, m_fCXBorder + kCornerEnlarge);
    if (rt.Contains(fx, fy))
      return FWL_WidgetHit::LeftTop;

    rt.Set(0, m_rtRelative.height - m_fCXBorder - kCornerEnlarge,
           m_fCYBorder + kCornerEnlarge, m_fCXBorder + kCornerEnlarge);
    if (rt.Contains(fx, fy))
      return FWL_WidgetHit::LeftBottom;

    rt.Set(m_rtRelative.width - m_fCYBorder - kCornerEnlarge, 0,
           m_fCYBorder + kCornerEnlarge, m_fCXBorder + kCornerEnlarge);
    if (rt.Contains(fx, fy))
      return FWL_WidgetHit::RightTop;

    rt.Set(m_rtRelative.width - m_fCYBorder - kCornerEnlarge,
           m_rtRelative.height - m_fCXBorder - kCornerEnlarge,
           m_fCYBorder + kCornerEnlarge, m_fCXBorder + kCornerEnlarge);
    if (rt.Contains(fx, fy))
      return FWL_WidgetHit::RightBottom;
  }
  return FWL_WidgetHit::Client;
}

void IFWL_Form::DrawWidget(CFX_Graphics* pGraphics, const CFX_Matrix* pMatrix) {
  if (!pGraphics)
    return;
  if (!m_pProperties->m_pThemeProvider)
    return;

  IFWL_ThemeProvider* pTheme = m_pProperties->m_pThemeProvider;
  bool bInactive = !IsActive();
  int32_t iState = bInactive ? CFWL_PartState_Inactive : CFWL_PartState_Normal;
  if ((m_pProperties->m_dwStyleExes & FWL_STYLEEXT_FRM_NoDrawClient) == 0)
    DrawBackground(pGraphics, pTheme);

#ifdef FWL_UseMacSystemBorder
  return;
#endif
  CFWL_ThemeBackground param;
  param.m_pWidget = this;
  param.m_dwStates = iState;
  param.m_pGraphics = pGraphics;
  param.m_rtPart = m_rtRelative;
  if (pMatrix)
    param.m_matrix.Concat(*pMatrix);
  if (m_pProperties->m_dwStyles & FWL_WGTSTYLE_Border) {
    param.m_iPart = CFWL_Part::Border;
    pTheme->DrawBackground(&param);
  }
  if ((m_pProperties->m_dwStyleExes & FWL_WGTSTYLE_EdgeMask) !=
      FWL_WGTSTYLE_EdgeNone) {
    CFX_RectF rtEdge;
    GetEdgeRect(rtEdge);
    param.m_iPart = CFWL_Part::Edge;
    param.m_rtPart = rtEdge;
    param.m_dwStates = iState;
    pTheme->DrawBackground(&param);
  }
  if (m_pProperties->m_dwStyles & FWL_WGTSTYLE_Caption) {
    param.m_iPart = CFWL_Part::Caption;
    param.m_dwStates = iState;
    param.m_rtPart = m_rtCaption;
    pTheme->DrawBackground(&param);
    DrawCaptionText(pGraphics, pTheme, pMatrix);
  } else if (m_pProperties->m_dwStyles & FWL_WGTSTYLE_NarrowCaption) {
    param.m_iPart = CFWL_Part::NarrowCaption;
    param.m_dwStates = iState;
    param.m_rtPart = m_rtCaption;
    pTheme->DrawBackground(&param);
    DrawCaptionText(pGraphics, pTheme, pMatrix);
  }
  if (m_pProperties->m_dwStyles & FWL_WGTSTYLE_Icon) {
    param.m_iPart = CFWL_Part::Icon;
    if (HasIcon())
      DrawIconImage(pGraphics, pTheme, pMatrix);
  }

#if (_FX_OS_ == _FX_MACOSX_)
  {
    if (m_pCloseBox) {
      param.m_iPart = CFWL_Part::CloseBox;
      param.m_dwStates = m_pCloseBox->GetPartState();
      if (m_pProperties->m_dwStates & FWL_WGTSTATE_Deactivated)
        param.m_dwStates = CFWL_PartState_Disabled;
      else if (CFWL_PartState_Normal == param.m_dwStates && m_bMouseIn)
        param.m_dwStates = CFWL_PartState_Hovered;
      param.m_rtPart = m_pCloseBox->m_rtBtn;
      pTheme->DrawBackground(&param);
    }
    if (m_pMaxBox) {
      param.m_iPart = CFWL_Part::MaximizeBox;
      param.m_dwStates = m_pMaxBox->GetPartState();
      if (m_pProperties->m_dwStates & FWL_WGTSTATE_Deactivated)
        param.m_dwStates = CFWL_PartState_Disabled;
      else if (CFWL_PartState_Normal == param.m_dwStates && m_bMouseIn)
        param.m_dwStates = CFWL_PartState_Hovered;
      param.m_rtPart = m_pMaxBox->m_rtBtn;
      param.m_bMaximize = m_bMaximized;
      pTheme->DrawBackground(&param);
    }
    if (m_pMinBox) {
      param.m_iPart = CFWL_Part::MinimizeBox;
      param.m_dwStates = m_pMinBox->GetPartState();
      if (m_pProperties->m_dwStates & FWL_WGTSTATE_Deactivated)
        param.m_dwStates = CFWL_PartState_Disabled;
      else if (CFWL_PartState_Normal == param.m_dwStates && m_bMouseIn)
        param.m_dwStates = CFWL_PartState_Hovered;
      param.m_rtPart = m_pMinBox->m_rtBtn;
      pTheme->DrawBackground(&param);
    }
    m_bMouseIn = false;
  }
#else
  {
    if (m_pCloseBox) {
      param.m_iPart = CFWL_Part::CloseBox;
      param.m_dwStates = m_pCloseBox->GetPartState();
      param.m_rtPart = m_pCloseBox->m_rtBtn;
      pTheme->DrawBackground(&param);
    }
    if (m_pMaxBox) {
      param.m_iPart = CFWL_Part::MaximizeBox;
      param.m_dwStates = m_pMaxBox->GetPartState();
      param.m_rtPart = m_pMaxBox->m_rtBtn;
      param.m_bMaximize = m_bMaximized;
      pTheme->DrawBackground(&param);
    }
    if (m_pMinBox) {
      param.m_iPart = CFWL_Part::MinimizeBox;
      param.m_dwStates = m_pMinBox->GetPartState();
      param.m_rtPart = m_pMinBox->m_rtBtn;
      pTheme->DrawBackground(&param);
    }
  }
#endif
}

IFWL_Widget* IFWL_Form::DoModal() {
  const IFWL_App* pApp = GetOwnerApp();
  if (!pApp)
    return nullptr;

  CFWL_NoteDriver* pDriver = pApp->GetNoteDriver();
  if (!pDriver)
    return nullptr;

  m_pNoteLoop = pdfium::MakeUnique<CFWL_NoteLoop>();
  m_pNoteLoop->SetMainForm(this);

  pDriver->PushNoteLoop(m_pNoteLoop.get());
  m_bDoModalFlag = true;
  SetStates(FWL_WGTSTATE_Invisible, false);
  pDriver->Run();

#if _FX_OS_ != _FX_MACOSX_
  pDriver->PopNoteLoop();
#endif

  m_pNoteLoop.reset();
  return nullptr;
}

void IFWL_Form::EndDoModal() {
  if (!m_pNoteLoop)
    return;

  m_bDoModalFlag = false;

#if (_FX_OS_ == _FX_MACOSX_)
  m_pNoteLoop->EndModalLoop();
  const IFWL_App* pApp = GetOwnerApp();
  if (!pApp)
    return;

  CFWL_NoteDriver* pDriver =
      static_cast<CFWL_NoteDriver*>(pApp->GetNoteDriver());
  if (!pDriver)
    return;

  pDriver->PopNoteLoop();
  SetStates(FWL_WGTSTATE_Invisible, true);
#else
  SetStates(FWL_WGTSTATE_Invisible, true);
  m_pNoteLoop->EndModalLoop();
#endif
}

void IFWL_Form::DrawBackground(CFX_Graphics* pGraphics,
                               IFWL_ThemeProvider* pTheme) {
  CFWL_ThemeBackground param;
  param.m_pWidget = this;
  param.m_iPart = CFWL_Part::Background;
  param.m_pGraphics = pGraphics;
  param.m_rtPart = m_rtRelative;
  param.m_rtPart.Deflate(m_fCYBorder, m_rtCaption.height, m_fCYBorder,
                         m_fCXBorder);
  pTheme->DrawBackground(&param);
}

void IFWL_Form::RemoveSysButtons() {
  m_rtCaption.Reset();
  delete m_pCloseBox;
  m_pCloseBox = nullptr;
  delete m_pMinBox;
  m_pMinBox = nullptr;
  delete m_pMaxBox;
  m_pMaxBox = nullptr;
  delete m_pCaptionBox;
  m_pCaptionBox = nullptr;
}

CFWL_SysBtn* IFWL_Form::GetSysBtnAtPoint(FX_FLOAT fx, FX_FLOAT fy) {
  if (m_pCloseBox && m_pCloseBox->m_rtBtn.Contains(fx, fy))
    return m_pCloseBox;
  if (m_pMaxBox && m_pMaxBox->m_rtBtn.Contains(fx, fy))
    return m_pMaxBox;
  if (m_pMinBox && m_pMinBox->m_rtBtn.Contains(fx, fy))
    return m_pMinBox;
  if (m_pCaptionBox && m_pCaptionBox->m_rtBtn.Contains(fx, fy))
    return m_pCaptionBox;
  return nullptr;
}

CFWL_SysBtn* IFWL_Form::GetSysBtnByState(uint32_t dwState) {
  if (m_pCloseBox && (m_pCloseBox->m_dwState & dwState))
    return m_pCloseBox;
  if (m_pMaxBox && (m_pMaxBox->m_dwState & dwState))
    return m_pMaxBox;
  if (m_pMinBox && (m_pMinBox->m_dwState & dwState))
    return m_pMinBox;
  if (m_pCaptionBox && (m_pCaptionBox->m_dwState & dwState))
    return m_pCaptionBox;
  return nullptr;
}

CFWL_SysBtn* IFWL_Form::GetSysBtnByIndex(int32_t nIndex) {
  if (nIndex < 0)
    return nullptr;

  CFX_ArrayTemplate<CFWL_SysBtn*> arrBtn;
  if (m_pMinBox)
    arrBtn.Add(m_pMinBox);
  if (m_pMaxBox)
    arrBtn.Add(m_pMaxBox);
  if (m_pCloseBox)
    arrBtn.Add(m_pCloseBox);
  return arrBtn[nIndex];
}

int32_t IFWL_Form::GetSysBtnIndex(CFWL_SysBtn* pBtn) {
  CFX_ArrayTemplate<CFWL_SysBtn*> arrBtn;
  if (m_pMinBox)
    arrBtn.Add(m_pMinBox);
  if (m_pMaxBox)
    arrBtn.Add(m_pMaxBox);
  if (m_pCloseBox)
    arrBtn.Add(m_pCloseBox);
  return arrBtn.Find(pBtn);
}

FX_FLOAT IFWL_Form::GetCaptionHeight() {
  CFWL_WidgetCapacity dwCapacity = CFWL_WidgetCapacity::None;

  if (m_pProperties->m_dwStyles & FWL_WGTSTYLE_Caption)
    dwCapacity = CFWL_WidgetCapacity::CYCaption;
  else if (m_pProperties->m_dwStyles & FWL_WGTSTYLE_NarrowCaption)
    dwCapacity = CFWL_WidgetCapacity::CYNarrowCaption;

  if (dwCapacity != CFWL_WidgetCapacity::None) {
    FX_FLOAT* pfCapHeight =
        static_cast<FX_FLOAT*>(GetThemeCapacity(dwCapacity));
    return pfCapHeight ? *pfCapHeight : 0;
  }
  return 0;
}

void IFWL_Form::DrawCaptionText(CFX_Graphics* pGs,
                                IFWL_ThemeProvider* pTheme,
                                const CFX_Matrix* pMatrix) {
  CFX_WideString wsText;
  IFWL_DataProvider* pData = m_pProperties->m_pDataProvider;
  pData->GetCaption(this, wsText);
  if (wsText.IsEmpty())
    return;

  CFWL_ThemeText textParam;
  textParam.m_pWidget = this;
  textParam.m_iPart = CFWL_Part::Caption;
  textParam.m_dwStates = CFWL_PartState_Normal;
  textParam.m_pGraphics = pGs;
  if (pMatrix)
    textParam.m_matrix.Concat(*pMatrix);

  CFX_RectF rtText;
  if (m_bCustomizeLayout) {
    rtText = m_rtCaptionText;
    rtText.top -= 5;
  } else {
    rtText = m_rtCaption;
    FX_FLOAT fpos;
    fpos = HasIcon() ? 29.0f : 13.0f;
    rtText.left += fpos;
  }
  textParam.m_rtPart = rtText;
  textParam.m_wsText = wsText;
  textParam.m_dwTTOStyles = FDE_TTOSTYLE_SingleLine | FDE_TTOSTYLE_Ellipsis;
  textParam.m_iTTOAlign = m_bCustomizeLayout ? FDE_TTOALIGNMENT_Center
                                             : FDE_TTOALIGNMENT_CenterLeft;
  pTheme->DrawText(&textParam);
}

void IFWL_Form::DrawIconImage(CFX_Graphics* pGs,
                              IFWL_ThemeProvider* pTheme,
                              const CFX_Matrix* pMatrix) {
  IFWL_FormDP* pData =
      static_cast<IFWL_FormDP*>(m_pProperties->m_pDataProvider);
  CFWL_ThemeBackground param;
  param.m_pWidget = this;
  param.m_iPart = CFWL_Part::Icon;
  param.m_pGraphics = pGs;
  param.m_pImage = pData->GetIcon(this, false);
  param.m_rtPart = m_rtIcon;
  if (pMatrix)
    param.m_matrix.Concat(*pMatrix);
  pTheme->DrawBackground(&param);
}

void IFWL_Form::GetEdgeRect(CFX_RectF& rtEdge) {
  rtEdge = m_rtRelative;
  if (m_pProperties->m_dwStyles & FWL_WGTSTYLE_Border) {
    FX_FLOAT fCX = GetBorderSize();
    FX_FLOAT fCY = GetBorderSize(false);
    rtEdge.Deflate(fCX, m_rtCaption.Height(), fCX, fCY);
  }
}

void IFWL_Form::SetWorkAreaRect() {
  m_rtRestore = m_pProperties->m_rtWidget;
  CFWL_WidgetMgr* pWidgetMgr = GetOwnerApp()->GetWidgetMgr();
  if (!pWidgetMgr)
    return;

  m_bSetMaximize = true;
  Repaint(&m_rtRelative);
}

void IFWL_Form::Layout() {
  GetRelativeRect(m_rtRelative);

#ifndef FWL_UseMacSystemBorder
  ResetSysBtn();
#endif
}

void IFWL_Form::ResetSysBtn() {
  m_fCXBorder =
      *static_cast<FX_FLOAT*>(GetThemeCapacity(CFWL_WidgetCapacity::CXBorder));
  m_fCYBorder =
      *static_cast<FX_FLOAT*>(GetThemeCapacity(CFWL_WidgetCapacity::CYBorder));
  RemoveSysButtons();

  IFWL_ThemeProvider* pTheme = m_pProperties->m_pThemeProvider;
  m_bCustomizeLayout = pTheme->IsCustomizedLayout(this);
  FX_FLOAT fCapHeight = GetCaptionHeight();
  if (fCapHeight > 0) {
    m_rtCaption = m_rtRelative;
    m_rtCaption.height = fCapHeight;
  }

  m_iSysBox = 0;
  if (m_pProperties->m_dwStyles & FWL_WGTSTYLE_CloseBox) {
    m_pCloseBox = new CFWL_SysBtn;
    if (!m_bCustomizeLayout) {
      m_pCloseBox->m_rtBtn.Set(
          m_rtRelative.right() - kSystemButtonMargin - kSystemButtonSize,
          kSystemButtonMargin, kSystemButtonSize, kSystemButtonSize);
    }
    m_iSysBox++;
  }
  if (m_pProperties->m_dwStyles & FWL_WGTSTYLE_MaximizeBox) {
    m_pMaxBox = new CFWL_SysBtn;
    if (!m_bCustomizeLayout) {
      if (m_pCloseBox) {
        m_pMaxBox->m_rtBtn.Set(
            m_pCloseBox->m_rtBtn.left - kSystemButtonSpan - kSystemButtonSize,
            m_pCloseBox->m_rtBtn.top, kSystemButtonSize, kSystemButtonSize);
      } else {
        m_pMaxBox->m_rtBtn.Set(
            m_rtRelative.right() - kSystemButtonMargin - kSystemButtonSize,
            kSystemButtonMargin, kSystemButtonSize, kSystemButtonSize);
      }
    }
    m_iSysBox++;
  }
  if (m_pProperties->m_dwStyles & FWL_WGTSTYLE_MinimizeBox) {
    m_pMinBox = new CFWL_SysBtn;
    if (!m_bCustomizeLayout) {
      if (m_pMaxBox) {
        m_pMinBox->m_rtBtn.Set(
            m_pMaxBox->m_rtBtn.left - kSystemButtonSpan - kSystemButtonSize,
            m_pMaxBox->m_rtBtn.top, kSystemButtonSize, kSystemButtonSize);
      } else if (m_pCloseBox) {
        m_pMinBox->m_rtBtn.Set(
            m_pCloseBox->m_rtBtn.left - kSystemButtonSpan - kSystemButtonSize,
            m_pCloseBox->m_rtBtn.top, kSystemButtonSize, kSystemButtonSize);
      } else {
        m_pMinBox->m_rtBtn.Set(
            m_rtRelative.right() - kSystemButtonMargin - kSystemButtonSize,
            kSystemButtonMargin, kSystemButtonSize, kSystemButtonSize);
      }
    }
    m_iSysBox++;
  }

  IFWL_FormDP* pData =
      static_cast<IFWL_FormDP*>(m_pProperties->m_pDataProvider);
  if (m_pProperties->m_dwStyles & FWL_WGTSTYLE_Icon &&
      pData->GetIcon(this, false)) {
    if (!m_bCustomizeLayout) {
      m_rtIcon.Set(5, (m_rtCaption.height - m_fSmallIconSz) / 2, m_fSmallIconSz,
                   m_fSmallIconSz);
    }
  }
}

void IFWL_Form::RegisterForm() {
  const IFWL_App* pApp = GetOwnerApp();
  if (!pApp)
    return;

  CFWL_NoteDriver* pDriver =
      static_cast<CFWL_NoteDriver*>(pApp->GetNoteDriver());
  if (!pDriver)
    return;

  pDriver->RegisterForm(this);
}

void IFWL_Form::UnRegisterForm() {
  const IFWL_App* pApp = GetOwnerApp();
  if (!pApp)
    return;

  CFWL_NoteDriver* pDriver =
      static_cast<CFWL_NoteDriver*>(pApp->GetNoteDriver());
  if (!pDriver)
    return;

  pDriver->UnRegisterForm(this);
}

void IFWL_Form::SetThemeData() {
  m_fSmallIconSz =
      *static_cast<FX_FLOAT*>(GetThemeCapacity(CFWL_WidgetCapacity::SmallIcon));
  m_fBigIconSz =
      *static_cast<FX_FLOAT*>(GetThemeCapacity(CFWL_WidgetCapacity::BigIcon));
}

bool IFWL_Form::HasIcon() {
  IFWL_FormDP* pData =
      static_cast<IFWL_FormDP*>(m_pProperties->m_pDataProvider);
  return !!pData->GetIcon(this, false);
}

void IFWL_Form::UpdateIcon() {
  CFWL_WidgetMgr* pWidgetMgr = GetOwnerApp()->GetWidgetMgr();
  if (!pWidgetMgr)
    return;

  IFWL_FormDP* pData =
      static_cast<IFWL_FormDP*>(m_pProperties->m_pDataProvider);
  CFX_DIBitmap* pBigIcon = pData->GetIcon(this, true);
  CFX_DIBitmap* pSmallIcon = pData->GetIcon(this, false);
  if (pBigIcon)
    m_pBigIcon = pBigIcon;
  if (pSmallIcon)
    m_pSmallIcon = pSmallIcon;
}

void IFWL_Form::UpdateCaption() {
  CFWL_WidgetMgr* pWidgetMgr = GetOwnerApp()->GetWidgetMgr();
  if (!pWidgetMgr)
    return;

  IFWL_DataProvider* pData = m_pProperties->m_pDataProvider;
  if (!pData)
    return;

  CFX_WideString text;
  pData->GetCaption(this, text);
}

void IFWL_Form::OnProcessMessage(CFWL_Message* pMessage) {
#ifndef FWL_UseMacSystemBorder
  if (!pMessage)
    return;

  switch (pMessage->GetClassID()) {
    case CFWL_MessageType::Mouse: {
      CFWL_MsgMouse* pMsg = static_cast<CFWL_MsgMouse*>(pMessage);
      switch (pMsg->m_dwCmd) {
        case FWL_MouseCommand::LeftButtonDown:
          OnLButtonDown(pMsg);
          break;
        case FWL_MouseCommand::LeftButtonUp:
          OnLButtonUp(pMsg);
          break;
        case FWL_MouseCommand::Move:
          OnMouseMove(pMsg);
          break;
        case FWL_MouseCommand::Leave:
          OnMouseLeave(pMsg);
          break;
        case FWL_MouseCommand::LeftButtonDblClk:
          OnLButtonDblClk(pMsg);
          break;
        default:
          break;
      }
      break;
    }
    default:
      break;
  }
#endif  // FWL_UseMacSystemBorder
}

void IFWL_Form::OnDrawWidget(CFX_Graphics* pGraphics,
                             const CFX_Matrix* pMatrix) {
  DrawWidget(pGraphics, pMatrix);
}

void IFWL_Form::OnLButtonDown(CFWL_MsgMouse* pMsg) {
  SetGrab(true);
  m_bLButtonDown = true;
  m_eResizeType = FORM_RESIZETYPE_None;

  CFWL_SysBtn* pPressBtn = GetSysBtnAtPoint(pMsg->m_fx, pMsg->m_fy);
  m_iCaptureBtn = GetSysBtnIndex(pPressBtn);

  CFX_RectF rtCap;
  rtCap.Set(m_rtCaption.left + m_fCYBorder, m_rtCaption.top + m_fCXBorder,
            m_rtCaption.width - kSystemButtonSize * m_iSysBox - 2 * m_fCYBorder,
            m_rtCaption.height - m_fCXBorder);

  if (pPressBtn) {
    pPressBtn->SetPressed();
    Repaint(&pPressBtn->m_rtBtn);
  } else if (rtCap.Contains(pMsg->m_fx, pMsg->m_fy)) {
    m_eResizeType = FORM_RESIZETYPE_Cap;
  }

  m_InfoStart.m_ptStart = CFX_PointF(pMsg->m_fx, pMsg->m_fy);
  m_InfoStart.m_szStart = CFX_SizeF(m_pProperties->m_rtWidget.width,
                                    m_pProperties->m_rtWidget.height);
}

void IFWL_Form::OnLButtonUp(CFWL_MsgMouse* pMsg) {
  SetGrab(false);
  m_bLButtonDown = false;
  CFWL_SysBtn* pPointBtn = GetSysBtnAtPoint(pMsg->m_fx, pMsg->m_fy);
  CFWL_SysBtn* pPressedBtn = GetSysBtnByIndex(m_iCaptureBtn);
  if (!pPressedBtn || pPointBtn != pPressedBtn)
    return;
  if (pPressedBtn == GetSysBtnByState(FWL_SYSBUTTONSTATE_Pressed))
    pPressedBtn->SetNormal();
  if (pPressedBtn == m_pMaxBox) {
    if (m_bMaximized) {
      SetWidgetRect(m_rtRestore);
      Update();
      Repaint();
    } else {
      SetWorkAreaRect();
      Update();
    }
    m_bMaximized = !m_bMaximized;
  } else if (pPressedBtn != m_pMinBox) {
    CFWL_EvtClose eClose;
    eClose.m_pSrcTarget = this;
    DispatchEvent(&eClose);
  }
}

void IFWL_Form::OnMouseMove(CFWL_MsgMouse* pMsg) {
  if (m_bLButtonDown)
    return;

  CFX_RectF rtInvalidate;
  rtInvalidate.Reset();
  CFWL_SysBtn* pPointBtn = GetSysBtnAtPoint(pMsg->m_fx, pMsg->m_fy);
  CFWL_SysBtn* pOldHover = GetSysBtnByState(FWL_SYSBUTTONSTATE_Hover);

#if _FX_OS_ == _FX_MACOSX_
  {
    if (pOldHover && pPointBtn != pOldHover)
      pOldHover->SetNormal();
    if (pPointBtn && pPointBtn != pOldHover)
      pPointBtn->SetHover();
    if (m_pCloseBox)
      rtInvalidate = m_pCloseBox->m_rtBtn;
    if (m_pMaxBox) {
      if (rtInvalidate.IsEmpty())
        rtInvalidate = m_pMaxBox->m_rtBtn;
      else
        rtInvalidate.Union(m_pMaxBox->m_rtBtn);
    }
    if (m_pMinBox) {
      if (rtInvalidate.IsEmpty())
        rtInvalidate = m_pMinBox->m_rtBtn;
      else
        rtInvalidate.Union(m_pMinBox->m_rtBtn);
    }
    if (!rtInvalidate.IsEmpty() &&
        rtInvalidate.Contains(pMsg->m_fx, pMsg->m_fy)) {
      m_bMouseIn = true;
    }
  }
#else
  {
    if (pOldHover && pPointBtn != pOldHover) {
      pOldHover->SetNormal();
      rtInvalidate = pOldHover->m_rtBtn;
    }
    if (pPointBtn && pPointBtn != pOldHover) {
      pPointBtn->SetHover();
      if (rtInvalidate.IsEmpty())
        rtInvalidate = pPointBtn->m_rtBtn;
      else
        rtInvalidate.Union(pPointBtn->m_rtBtn);
    }
  }
#endif

  if (!rtInvalidate.IsEmpty())
    Repaint(&rtInvalidate);
}

void IFWL_Form::OnMouseLeave(CFWL_MsgMouse* pMsg) {
  CFWL_SysBtn* pHover = GetSysBtnByState(FWL_SYSBUTTONSTATE_Hover);
  if (!pHover)
    return;

  pHover->SetNormal();
  Repaint(&pHover->m_rtBtn);
}

void IFWL_Form::OnLButtonDblClk(CFWL_MsgMouse* pMsg) {
  if ((m_pProperties->m_dwStyleExes & FWL_STYLEEXT_FRM_Resize) &&
      HitTest(pMsg->m_fx, pMsg->m_fy) == FWL_WidgetHit::Titlebar) {
    if (m_bMaximized)
      SetWidgetRect(m_rtRestore);
    else
      SetWorkAreaRect();

    Update();
    m_bMaximized = !m_bMaximized;
  }
}

CFWL_SysBtn::CFWL_SysBtn() {
  m_rtBtn.Set(0, 0, 0, 0);
  m_dwState = 0;
}

bool CFWL_SysBtn::IsDisabled() const {
  return !!(m_dwState & FWL_SYSBUTTONSTATE_Disabled);
}

void CFWL_SysBtn::SetNormal() {
  m_dwState &= 0xFFF0;
}

void CFWL_SysBtn::SetPressed() {
  SetNormal();
  m_dwState |= FWL_SYSBUTTONSTATE_Pressed;
}

void CFWL_SysBtn::SetHover() {
  SetNormal();
  m_dwState |= FWL_SYSBUTTONSTATE_Hover;
}

void CFWL_SysBtn::SetDisabled(bool bDisabled) {
  bDisabled ? m_dwState |= FWL_SYSBUTTONSTATE_Disabled
            : m_dwState &= ~FWL_SYSBUTTONSTATE_Disabled;
}

uint32_t CFWL_SysBtn::GetPartState() const {
  if (IsDisabled())
    return CFWL_PartState_Disabled;
  if (m_dwState & FWL_SYSBUTTONSTATE_Pressed)
    return CFWL_PartState_Pressed;
  if (m_dwState & FWL_SYSBUTTONSTATE_Hover)
    return CFWL_PartState_Hovered;
  return CFWL_PartState_Normal;
}
