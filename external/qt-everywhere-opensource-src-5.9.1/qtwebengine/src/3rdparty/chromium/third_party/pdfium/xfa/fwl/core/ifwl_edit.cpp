// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/ifwl_edit.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"
#include "xfa/fde/cfde_txtedtengine.h"
#include "xfa/fde/fde_gedevice.h"
#include "xfa/fde/fde_render.h"
#include "xfa/fde/ifde_txtedtpage.h"
#include "xfa/fgas/font/fgas_gefont.h"
#include "xfa/fwl/core/cfwl_evtcheckword.h"
#include "xfa/fwl/core/cfwl_evttextchanged.h"
#include "xfa/fwl/core/cfwl_evttextfull.h"
#include "xfa/fwl/core/cfwl_evtvalidate.h"
#include "xfa/fwl/core/cfwl_msgkey.h"
#include "xfa/fwl/core/cfwl_msgmouse.h"
#include "xfa/fwl/core/cfwl_themebackground.h"
#include "xfa/fwl/core/cfwl_themepart.h"
#include "xfa/fwl/core/cfwl_widgetmgr.h"
#include "xfa/fwl/core/ifwl_app.h"
#include "xfa/fwl/core/ifwl_caret.h"
#include "xfa/fwl/core/ifwl_themeprovider.h"
#include "xfa/fxfa/xfa_ffdoc.h"
#include "xfa/fxfa/xfa_ffwidget.h"
#include "xfa/fxgraphics/cfx_path.h"

namespace {

const int kEditMargin = 3;

bool FX_EDIT_ISLATINWORD(FX_WCHAR c) {
  return c == 0x2D || (c <= 0x005A && c >= 0x0041) ||
         (c <= 0x007A && c >= 0x0061) || (c <= 0x02AF && c >= 0x00C0) ||
         c == 0x0027;
}

void AddSquigglyPath(CFX_Path* pPathData,
                     FX_FLOAT fStartX,
                     FX_FLOAT fEndX,
                     FX_FLOAT fY,
                     FX_FLOAT fStep) {
  pPathData->MoveTo(fStartX, fY);
  int i = 1;
  for (FX_FLOAT fx = fStartX + fStep; fx < fEndX; fx += fStep, ++i)
    pPathData->LineTo(fx, fY + (i & 1) * fStep);
}

}  // namespace

IFWL_Edit::IFWL_Edit(const IFWL_App* app,
                     std::unique_ptr<CFWL_WidgetProperties> properties,
                     IFWL_Widget* pOuter)
    : IFWL_Widget(app, std::move(properties), pOuter),
      m_fVAlignOffset(0.0f),
      m_fScrollOffsetX(0.0f),
      m_fScrollOffsetY(0.0f),
      m_bLButtonDown(false),
      m_nSelStart(0),
      m_nLimit(-1),
      m_fFontSize(0),
      m_bSetRange(false),
      m_iMax(0xFFFFFFF),
      m_iCurRecord(-1),
      m_iMaxRecord(128) {
  m_rtClient.Reset();
  m_rtEngine.Reset();
  m_rtStatic.Reset();

  InitCaret();
}

IFWL_Edit::~IFWL_Edit() {
  if (m_pProperties->m_dwStates & FWL_WGTSTATE_Focused)
    ShowCaret(false);
  ClearRecord();
}

FWL_Type IFWL_Edit::GetClassID() const {
  return FWL_Type::Edit;
}

void IFWL_Edit::GetWidgetRect(CFX_RectF& rect, bool bAutoSize) {
  if (!bAutoSize) {
    rect = m_pProperties->m_rtWidget;
    if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_OuterScrollbar) {
      if (IsShowScrollBar(true)) {
        FX_FLOAT* pfWidth = static_cast<FX_FLOAT*>(
            GetThemeCapacity(CFWL_WidgetCapacity::ScrollBarWidth));
        rect.width += *pfWidth;
        rect.width += kEditMargin;
      }
      if (IsShowScrollBar(false)) {
        FX_FLOAT* pfWidth = static_cast<FX_FLOAT*>(
            GetThemeCapacity(CFWL_WidgetCapacity::ScrollBarWidth));
        rect.height += *pfWidth;
        rect.height += kEditMargin;
      }
    }
    return;
  }

  rect.Set(0, 0, 0, 0);

  int32_t iTextLen = m_EdtEngine.GetTextLength();
  if (iTextLen > 0) {
    CFX_WideString wsText;
    m_EdtEngine.GetText(wsText, 0);
    CFX_SizeF sz = CalcTextSize(
        wsText, m_pProperties->m_pThemeProvider,
        !!(m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_MultiLine));
    rect.Set(0, 0, sz.x, sz.y);
  }
  IFWL_Widget::GetWidgetRect(rect, true);
}

void IFWL_Edit::SetStates(uint32_t dwStates, bool bSet) {
  if ((m_pProperties->m_dwStates & FWL_WGTSTATE_Invisible) ||
      (m_pProperties->m_dwStates & FWL_WGTSTATE_Disabled)) {
    ShowCaret(false);
  }
  IFWL_Widget::SetStates(dwStates, bSet);
}

void IFWL_Edit::Update() {
  if (IsLocked())
    return;
  if (!m_pProperties->m_pThemeProvider)
    m_pProperties->m_pThemeProvider = GetAvailableTheme();

  Layout();
  if (m_rtClient.IsEmpty())
    return;

  UpdateEditEngine();
  UpdateVAlignment();
  UpdateScroll();
  InitCaret();
}

FWL_WidgetHit IFWL_Edit::HitTest(FX_FLOAT fx, FX_FLOAT fy) {
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_OuterScrollbar) {
    if (IsShowScrollBar(true)) {
      CFX_RectF rect;
      m_pVertScrollBar->GetWidgetRect(rect);
      if (rect.Contains(fx, fy))
        return FWL_WidgetHit::VScrollBar;
    }
    if (IsShowScrollBar(false)) {
      CFX_RectF rect;
      m_pHorzScrollBar->GetWidgetRect(rect);
      if (rect.Contains(fx, fy))
        return FWL_WidgetHit::HScrollBar;
    }
  }
  if (m_rtClient.Contains(fx, fy))
    return FWL_WidgetHit::Edit;
  return FWL_WidgetHit::Unknown;
}

void IFWL_Edit::AddSpellCheckObj(CFX_Path& PathData,
                                 int32_t nStart,
                                 int32_t nCount,
                                 FX_FLOAT fOffSetX,
                                 FX_FLOAT fOffSetY) {
  FX_FLOAT fStartX = 0.0f;
  FX_FLOAT fEndX = 0.0f;
  FX_FLOAT fY = 0.0f;
  FX_FLOAT fStep = 0.0f;
  IFDE_TxtEdtPage* pPage = m_EdtEngine.GetPage(0);
  CFX_RectFArray rectArray;
  CFX_RectF rectText;
  const FDE_TXTEDTPARAMS* txtEdtParams = m_EdtEngine.GetEditParams();
  FX_FLOAT fAsent = static_cast<FX_FLOAT>(txtEdtParams->pFont->GetAscent()) *
                    txtEdtParams->fFontSize / 1000;
  pPage->CalcRangeRectArray(nStart, nCount, rectArray);

  for (int i = 0; i < rectArray.GetSize(); i++) {
    rectText = rectArray.GetAt(i);
    fY = rectText.top + fAsent + fOffSetY;
    fStep = txtEdtParams->fFontSize / 16.0f;
    fStartX = rectText.left + fOffSetX;
    fEndX = fStartX + rectText.Width();
    AddSquigglyPath(&PathData, fStartX, fEndX, fY, fStep);
  }
}

void IFWL_Edit::DrawSpellCheck(CFX_Graphics* pGraphics,
                               const CFX_Matrix* pMatrix) {
  pGraphics->SaveGraphState();
  if (pMatrix)
    pGraphics->ConcatMatrix(const_cast<CFX_Matrix*>(pMatrix));

  CFX_Color crLine(0xFFFF0000);
  CFWL_EvtCheckWord checkWordEvent;
  checkWordEvent.m_pSrcTarget = this;

  CFX_ByteString sLatinWord;
  CFX_Path pathSpell;
  pathSpell.Create();

  int32_t nStart = 0;
  FX_FLOAT fOffSetX = m_rtEngine.left - m_fScrollOffsetX;
  FX_FLOAT fOffSetY = m_rtEngine.top - m_fScrollOffsetY + m_fVAlignOffset;

  CFX_WideString wsSpell;
  GetText(wsSpell);
  int32_t nContentLen = wsSpell.GetLength();
  for (int i = 0; i < nContentLen; i++) {
    if (FX_EDIT_ISLATINWORD(wsSpell[i])) {
      if (sLatinWord.IsEmpty())
        nStart = i;
      sLatinWord += (FX_CHAR)wsSpell[i];
      continue;
    }
    checkWordEvent.bsWord = sLatinWord;
    checkWordEvent.bCheckWord = true;
    DispatchEvent(&checkWordEvent);

    if (!sLatinWord.IsEmpty() && !checkWordEvent.bCheckWord) {
      AddSpellCheckObj(pathSpell, nStart, sLatinWord.GetLength(), fOffSetX,
                       fOffSetY);
    }
    sLatinWord.clear();
  }

  checkWordEvent.bsWord = sLatinWord;
  checkWordEvent.bCheckWord = true;
  DispatchEvent(&checkWordEvent);

  if (!sLatinWord.IsEmpty() && !checkWordEvent.bCheckWord) {
    AddSpellCheckObj(pathSpell, nStart, sLatinWord.GetLength(), fOffSetX,
                     fOffSetY);
  }
  if (!pathSpell.IsEmpty()) {
    CFX_RectF rtClip = m_rtEngine;
    CFX_Matrix mt;
    mt.Set(1, 0, 0, 1, fOffSetX, fOffSetY);
    if (pMatrix) {
      pMatrix->TransformRect(rtClip);
      mt.Concat(*pMatrix);
    }
    pGraphics->SetClipRect(rtClip);
    pGraphics->SetStrokeColor(&crLine);
    pGraphics->SetLineWidth(0);
    pGraphics->StrokePath(&pathSpell, nullptr);
  }
  pGraphics->RestoreGraphState();
}

void IFWL_Edit::DrawWidget(CFX_Graphics* pGraphics, const CFX_Matrix* pMatrix) {
  if (!pGraphics)
    return;
  if (!m_pProperties->m_pThemeProvider)
    return;
  if (m_rtClient.IsEmpty())
    return;

  IFWL_ThemeProvider* pTheme = m_pProperties->m_pThemeProvider;
  if (!m_pWidgetMgr->IsFormDisabled())
    DrawTextBk(pGraphics, pTheme, pMatrix);
  DrawContent(pGraphics, pTheme, pMatrix);

  if ((m_pProperties->m_dwStates & FWL_WGTSTATE_Focused) &&
      !(m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_ReadOnly)) {
    DrawSpellCheck(pGraphics, pMatrix);
  }
  if (HasBorder())
    DrawBorder(pGraphics, CFWL_Part::Border, pTheme, pMatrix);
  if (HasEdge())
    DrawEdge(pGraphics, CFWL_Part::Edge, pTheme, pMatrix);
}

void IFWL_Edit::SetThemeProvider(IFWL_ThemeProvider* pThemeProvider) {
  if (!pThemeProvider)
    return;
  if (m_pHorzScrollBar)
    m_pHorzScrollBar->SetThemeProvider(pThemeProvider);
  if (m_pVertScrollBar)
    m_pVertScrollBar->SetThemeProvider(pThemeProvider);
  if (m_pCaret)
    m_pCaret->SetThemeProvider(pThemeProvider);
  m_pProperties->m_pThemeProvider = pThemeProvider;
}

void IFWL_Edit::SetText(const CFX_WideString& wsText) {
  m_EdtEngine.SetText(wsText);
}

int32_t IFWL_Edit::GetTextLength() const {
  return m_EdtEngine.GetTextLength();
}

void IFWL_Edit::GetText(CFX_WideString& wsText,
                        int32_t nStart,
                        int32_t nCount) const {
  m_EdtEngine.GetText(wsText, nStart, nCount);
}

void IFWL_Edit::ClearText() {
  m_EdtEngine.ClearText();
}

void IFWL_Edit::AddSelRange(int32_t nStart, int32_t nCount) {
  m_EdtEngine.AddSelRange(nStart, nCount);
}

int32_t IFWL_Edit::CountSelRanges() const {
  return m_EdtEngine.CountSelRanges();
}

int32_t IFWL_Edit::GetSelRange(int32_t nIndex, int32_t& nStart) const {
  return m_EdtEngine.GetSelRange(nIndex, nStart);
}

void IFWL_Edit::ClearSelections() {
  m_EdtEngine.ClearSelection();
}

int32_t IFWL_Edit::GetLimit() const {
  return m_nLimit;
}

void IFWL_Edit::SetLimit(int32_t nLimit) {
  m_nLimit = nLimit;
  m_EdtEngine.SetLimit(nLimit);
}

void IFWL_Edit::SetAliasChar(FX_WCHAR wAlias) {
  m_EdtEngine.SetAliasChar(wAlias);
}

bool IFWL_Edit::Copy(CFX_WideString& wsCopy) {
  int32_t nCount = m_EdtEngine.CountSelRanges();
  if (nCount == 0)
    return false;

  wsCopy.clear();
  CFX_WideString wsTemp;
  int32_t nStart, nLength;
  for (int32_t i = 0; i < nCount; i++) {
    nLength = m_EdtEngine.GetSelRange(i, nStart);
    m_EdtEngine.GetText(wsTemp, nStart, nLength);
    wsCopy += wsTemp;
    wsTemp.clear();
  }
  return true;
}

bool IFWL_Edit::Cut(CFX_WideString& wsCut) {
  int32_t nCount = m_EdtEngine.CountSelRanges();
  if (nCount == 0)
    return false;

  wsCut.clear();
  CFX_WideString wsTemp;
  int32_t nStart, nLength;
  for (int32_t i = 0; i < nCount; i++) {
    nLength = m_EdtEngine.GetSelRange(i, nStart);
    m_EdtEngine.GetText(wsTemp, nStart, nLength);
    wsCut += wsTemp;
    wsTemp.clear();
  }
  m_EdtEngine.Delete(0);
  return true;
}

bool IFWL_Edit::Paste(const CFX_WideString& wsPaste) {
  int32_t nCaret = m_EdtEngine.GetCaretPos();
  int32_t iError =
      m_EdtEngine.Insert(nCaret, wsPaste.c_str(), wsPaste.GetLength());
  if (iError < 0) {
    ProcessInsertError(iError);
    return false;
  }
  return true;
}

bool IFWL_Edit::Redo(const IFDE_TxtEdtDoRecord* pRecord) {
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_NoRedoUndo)
    return true;
  return m_EdtEngine.Redo(pRecord);
}

bool IFWL_Edit::Undo(const IFDE_TxtEdtDoRecord* pRecord) {
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_NoRedoUndo)
    return true;
  return m_EdtEngine.Undo(pRecord);
}

bool IFWL_Edit::Undo() {
  if (!CanUndo())
    return false;
  return Undo(m_DoRecords[m_iCurRecord--].get());
}

bool IFWL_Edit::Redo() {
  if (!CanRedo())
    return false;
  return Redo(m_DoRecords[++m_iCurRecord].get());
}

bool IFWL_Edit::CanUndo() {
  return m_iCurRecord >= 0;
}

bool IFWL_Edit::CanRedo() {
  return m_iCurRecord < pdfium::CollectionSize<int32_t>(m_DoRecords) - 1;
}

void IFWL_Edit::SetOuter(IFWL_Widget* pOuter) {
  m_pOuter = pOuter;
}

void IFWL_Edit::On_CaretChanged(CFDE_TxtEdtEngine* pEdit,
                                int32_t nPage,
                                bool bVisible) {
  if (m_rtEngine.IsEmpty())
    return;
  if ((m_pProperties->m_dwStates & FWL_WGTSTATE_Focused) == 0)
    return;

  bool bRepaintContent = UpdateOffset();
  UpdateCaret();
  CFX_RectF rtInvalid;
  rtInvalid.Set(0, 0, 0, 0);
  bool bRepaintScroll = false;
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_MultiLine) {
    IFWL_ScrollBar* pScroll = UpdateScroll();
    if (pScroll) {
      pScroll->GetWidgetRect(rtInvalid);
      bRepaintScroll = true;
    }
  }
  if (bRepaintContent || bRepaintScroll) {
    if (bRepaintContent)
      rtInvalid.Union(m_rtEngine);
    Repaint(&rtInvalid);
  }
}

void IFWL_Edit::On_TextChanged(CFDE_TxtEdtEngine* pEdit,
                               FDE_TXTEDT_TEXTCHANGE_INFO& ChangeInfo) {
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_VAlignMask)
    UpdateVAlignment();

  CFX_RectF rtTemp;
  GetClientRect(rtTemp);

  CFWL_EvtTextChanged event;
  event.m_pSrcTarget = this;
  event.nChangeType = ChangeInfo.nChangeType;
  event.wsInsert = ChangeInfo.wsInsert;
  event.wsDelete = ChangeInfo.wsDelete;
  event.wsPrevText = ChangeInfo.wsPrevText;
  DispatchEvent(&event);

  LayoutScrollBar();
  Repaint(&rtTemp);
}

void IFWL_Edit::On_SelChanged(CFDE_TxtEdtEngine* pEdit) {
  CFX_RectF rtTemp;
  GetClientRect(rtTemp);
  Repaint(&rtTemp);
}

bool IFWL_Edit::On_PageLoad(CFDE_TxtEdtEngine* pEdit,
                            int32_t nPageIndex,
                            int32_t nPurpose) {
  IFDE_TxtEdtPage* pPage = m_EdtEngine.GetPage(nPageIndex);
  if (!pPage)
    return false;

  pPage->LoadPage(nullptr, nullptr);
  return true;
}

bool IFWL_Edit::On_PageUnload(CFDE_TxtEdtEngine* pEdit,
                              int32_t nPageIndex,
                              int32_t nPurpose) {
  IFDE_TxtEdtPage* pPage = m_EdtEngine.GetPage(nPageIndex);
  if (!pPage)
    return false;

  pPage->UnloadPage(nullptr);
  return true;
}

void IFWL_Edit::On_AddDoRecord(CFDE_TxtEdtEngine* pEdit,
                               IFDE_TxtEdtDoRecord* pRecord) {
  AddDoRecord(pRecord);
}

bool IFWL_Edit::On_Validate(CFDE_TxtEdtEngine* pEdit, CFX_WideString& wsText) {
  IFWL_Widget* pDst = GetOuter();
  if (!pDst)
    pDst = this;

  CFWL_EvtValidate event;
  event.pDstWidget = pDst;
  event.m_pSrcTarget = this;
  event.wsInsert = wsText;
  event.bValidate = true;
  DispatchEvent(&event);
  return event.bValidate;
}

void IFWL_Edit::SetScrollOffset(FX_FLOAT fScrollOffset) {
  m_fScrollOffsetY = fScrollOffset;
}

void IFWL_Edit::DrawTextBk(CFX_Graphics* pGraphics,
                           IFWL_ThemeProvider* pTheme,
                           const CFX_Matrix* pMatrix) {
  CFWL_ThemeBackground param;
  param.m_pWidget = this;
  param.m_iPart = CFWL_Part::Background;
  param.m_bStaticBackground = false;
  param.m_dwStates = m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_ReadOnly
                         ? CFWL_PartState_ReadOnly
                         : CFWL_PartState_Normal;
  uint32_t dwStates = (m_pProperties->m_dwStates & FWL_WGTSTATE_Disabled);
  if (dwStates)
    param.m_dwStates = CFWL_PartState_Disabled;
  param.m_pGraphics = pGraphics;
  param.m_matrix = *pMatrix;
  param.m_rtPart = m_rtClient;
  pTheme->DrawBackground(&param);

  if (!IsShowScrollBar(true) || !IsShowScrollBar(false))
    return;

  CFX_RectF rtScorll;
  m_pHorzScrollBar->GetWidgetRect(rtScorll);

  CFX_RectF rtStatic;
  rtStatic.Set(m_rtClient.right() - rtScorll.height,
               m_rtClient.bottom() - rtScorll.height, rtScorll.height,
               rtScorll.height);
  param.m_bStaticBackground = true;
  param.m_bMaximize = true;
  param.m_rtPart = rtStatic;
  pTheme->DrawBackground(&param);
}

void IFWL_Edit::DrawContent(CFX_Graphics* pGraphics,
                            IFWL_ThemeProvider* pTheme,
                            const CFX_Matrix* pMatrix) {
  IFDE_TxtEdtPage* pPage = m_EdtEngine.GetPage(0);
  if (!pPage)
    return;

  pGraphics->SaveGraphState();
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_CombText)
    pGraphics->SaveGraphState();

  CFX_RectF rtClip = m_rtEngine;
  FX_FLOAT fOffSetX = m_rtEngine.left - m_fScrollOffsetX;
  FX_FLOAT fOffSetY = m_rtEngine.top - m_fScrollOffsetY + m_fVAlignOffset;
  CFX_Matrix mt;
  mt.Set(1, 0, 0, 1, fOffSetX, fOffSetY);
  if (pMatrix) {
    pMatrix->TransformRect(rtClip);
    mt.Concat(*pMatrix);
  }

  bool bShowSel = (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_NoHideSel) ||
                  (m_pProperties->m_dwStates & FWL_WGTSTATE_Focused);
  if (bShowSel) {
    IFWL_Widget* pForm = m_pWidgetMgr->GetSystemFormWidget(this);
    if (pForm) {
      bShowSel = (pForm->GetStates() & FWL_WGTSTATE_Deactivated) !=
                 FWL_WGTSTATE_Deactivated;
    }
  }

  int32_t nSelCount = m_EdtEngine.CountSelRanges();
  if (bShowSel && nSelCount > 0) {
    int32_t nPageCharStart = pPage->GetCharStart();
    int32_t nPageCharCount = pPage->GetCharCount();
    int32_t nPageCharEnd = nPageCharStart + nPageCharCount - 1;
    int32_t nCharCount;
    int32_t nCharStart;
    CFX_RectFArray rectArr;
    int32_t i = 0;
    for (i = 0; i < nSelCount; i++) {
      nCharCount = m_EdtEngine.GetSelRange(i, nCharStart);
      int32_t nCharEnd = nCharStart + nCharCount - 1;
      if (nCharEnd < nPageCharStart || nCharStart > nPageCharEnd)
        continue;

      int32_t nBgn = std::max(nCharStart, nPageCharStart);
      int32_t nEnd = std::min(nCharEnd, nPageCharEnd);
      pPage->CalcRangeRectArray(nBgn - nPageCharStart, nEnd - nBgn + 1,
                                rectArr);
    }

    int32_t nCount = rectArr.GetSize();
    CFX_Path path;
    path.Create();
    for (i = 0; i < nCount; i++) {
      rectArr[i].left += fOffSetX;
      rectArr[i].top += fOffSetY;
      path.AddRectangle(rectArr[i].left, rectArr[i].top, rectArr[i].width,
                        rectArr[i].height);
    }
    pGraphics->SetClipRect(rtClip);

    CFWL_ThemeBackground param;
    param.m_pGraphics = pGraphics;
    param.m_matrix = *pMatrix;
    param.m_pWidget = this;
    param.m_iPart = CFWL_Part::Background;
    param.m_pPath = &path;
    pTheme->DrawBackground(&param);
  }

  CFX_RenderDevice* pRenderDev = pGraphics->GetRenderDevice();
  if (!pRenderDev)
    return;

  std::unique_ptr<CFDE_RenderDevice> pRenderDevice(
      new CFDE_RenderDevice(pRenderDev, false));
  std::unique_ptr<CFDE_RenderContext> pRenderContext(new CFDE_RenderContext);
  pRenderDevice->SetClipRect(rtClip);
  pRenderContext->StartRender(pRenderDevice.get(), pPage, mt);
  pRenderContext->DoRender(nullptr);

  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_CombText) {
    pGraphics->RestoreGraphState();
    CFX_Path path;
    path.Create();
    int32_t iLimit = m_nLimit > 0 ? m_nLimit : 1;
    FX_FLOAT fStep = m_rtEngine.width / iLimit;
    FX_FLOAT fLeft = m_rtEngine.left + 1;
    for (int32_t i = 1; i < iLimit; i++) {
      fLeft += fStep;
      path.AddLine(fLeft, m_rtClient.top, fLeft, m_rtClient.bottom());
    }

    CFWL_ThemeBackground param;
    param.m_pGraphics = pGraphics;
    param.m_matrix = *pMatrix;
    param.m_pWidget = this;
    param.m_iPart = CFWL_Part::CombTextLine;
    param.m_pPath = &path;
    pTheme->DrawBackground(&param);
  }
  pGraphics->RestoreGraphState();
}

void IFWL_Edit::UpdateEditEngine() {
  UpdateEditParams();
  UpdateEditLayout();
  if (m_nLimit > -1)
    m_EdtEngine.SetLimit(m_nLimit);
}

void IFWL_Edit::UpdateEditParams() {
  FDE_TXTEDTPARAMS params;
  params.nHorzScale = 100;
  params.fPlateWidth = m_rtEngine.width;
  params.fPlateHeight = m_rtEngine.height;
  if (m_pProperties->m_dwStyles & FWL_WGTSTYLE_RTLLayout)
    params.dwLayoutStyles |= FDE_TEXTEDITLAYOUT_RTL;
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_VerticalLayout)
    params.dwLayoutStyles |= FDE_TEXTEDITLAYOUT_DocVertical;
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_VerticalChars)
    params.dwLayoutStyles |= FDE_TEXTEDITLAYOUT_CharVertial;
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_ReverseLine)
    params.dwLayoutStyles |= FDE_TEXTEDITLAYOUT_LineReserve;
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_ArabicShapes)
    params.dwLayoutStyles |= FDE_TEXTEDITLAYOUT_ArabicShapes;
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_ExpandTab)
    params.dwLayoutStyles |= FDE_TEXTEDITLAYOUT_ExpandTab;
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_CombText)
    params.dwLayoutStyles |= FDE_TEXTEDITLAYOUT_CombText;
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_LastLineHeight)
    params.dwLayoutStyles |= FDE_TEXTEDITLAYOUT_LastLineHeight;
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_Validate)
    params.dwMode |= FDE_TEXTEDITMODE_Validate;
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_Password)
    params.dwMode |= FDE_TEXTEDITMODE_Password;

  switch (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_HAlignMask) {
    case FWL_STYLEEXT_EDT_HNear: {
      params.dwAlignment |= FDE_TEXTEDITALIGN_Left;
      break;
    }
    case FWL_STYLEEXT_EDT_HCenter: {
      params.dwAlignment |= FDE_TEXTEDITALIGN_Center;
      break;
    }
    case FWL_STYLEEXT_EDT_HFar: {
      params.dwAlignment |= FDE_TEXTEDITALIGN_Right;
      break;
    }
    default:
      break;
  }
  switch (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_HAlignModeMask) {
    case FWL_STYLEEXT_EDT_Justified: {
      params.dwAlignment |= FDE_TEXTEDITALIGN_Justified;
      break;
    }
    case FWL_STYLEEXT_EDT_Distributed: {
      params.dwAlignment |= FDE_TEXTEDITALIGN_Distributed;
      break;
    }
    default: {
      params.dwAlignment |= FDE_TEXTEDITALIGN_Normal;
      break;
    }
  }
  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_MultiLine) {
    params.dwMode |= FDE_TEXTEDITMODE_MultiLines;
    if ((m_pProperties->m_dwStyles & FWL_WGTSTYLE_HScroll) == 0 &&
        (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_AutoHScroll) == 0) {
      params.dwMode |=
          FDE_TEXTEDITMODE_AutoLineWrap | FDE_TEXTEDITMODE_LimitArea_Horz;
    }
    if ((m_pProperties->m_dwStyles & FWL_WGTSTYLE_VScroll) == 0 &&
        (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_AutoVScroll) == 0) {
      params.dwMode |= FDE_TEXTEDITMODE_LimitArea_Vert;
    } else {
      params.fPlateHeight = 0x00FFFFFF;
    }
  } else if ((m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_AutoHScroll) ==
             0) {
    params.dwMode |= FDE_TEXTEDITMODE_LimitArea_Horz;
  }
  if ((m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_ReadOnly) ||
      (m_pProperties->m_dwStates & FWL_WGTSTATE_Disabled)) {
    params.dwMode |= FDE_TEXTEDITMODE_ReadOnly;
  }

  FX_FLOAT* pFontSize =
      static_cast<FX_FLOAT*>(GetThemeCapacity(CFWL_WidgetCapacity::FontSize));
  if (!pFontSize)
    return;

  m_fFontSize = *pFontSize;
  uint32_t* pFontColor =
      static_cast<uint32_t*>(GetThemeCapacity(CFWL_WidgetCapacity::TextColor));
  if (!pFontColor)
    return;

  params.dwFontColor = *pFontColor;
  FX_FLOAT* pLineHeight =
      static_cast<FX_FLOAT*>(GetThemeCapacity(CFWL_WidgetCapacity::LineHeight));
  if (!pLineHeight)
    return;

  params.fLineSpace = *pLineHeight;
  CFGAS_GEFont* pFont =
      static_cast<CFGAS_GEFont*>(GetThemeCapacity(CFWL_WidgetCapacity::Font));
  if (!pFont)
    return;

  params.pFont = pFont;
  params.fFontSize = m_fFontSize;
  params.nLineCount = (int32_t)(params.fPlateHeight / params.fLineSpace);
  if (params.nLineCount <= 0)
    params.nLineCount = 1;
  params.fTabWidth = params.fFontSize * 1;
  params.bTabEquidistant = true;
  params.wLineBreakChar = L'\n';
  params.nCharRotation = 0;
  params.pEventSink = this;
  m_EdtEngine.SetEditParams(params);
}

void IFWL_Edit::UpdateEditLayout() {
  if (m_EdtEngine.GetTextLength() <= 0)
    m_EdtEngine.SetTextByStream(nullptr);

  IFDE_TxtEdtPage* pPage = m_EdtEngine.GetPage(0);
  if (pPage)
    pPage->UnloadPage(nullptr);

  m_EdtEngine.StartLayout();
  m_EdtEngine.DoLayout(nullptr);
  m_EdtEngine.EndLayout();
  pPage = m_EdtEngine.GetPage(0);
  if (pPage)
    pPage->LoadPage(nullptr, nullptr);
}

bool IFWL_Edit::UpdateOffset() {
  CFX_RectF rtCaret;
  m_EdtEngine.GetCaretRect(rtCaret);
  FX_FLOAT fOffSetX = m_rtEngine.left - m_fScrollOffsetX;
  FX_FLOAT fOffSetY = m_rtEngine.top - m_fScrollOffsetY + m_fVAlignOffset;
  rtCaret.Offset(fOffSetX, fOffSetY);
  const CFX_RectF& rtEidt = m_rtEngine;
  if (rtEidt.Contains(rtCaret)) {
    IFDE_TxtEdtPage* pPage = m_EdtEngine.GetPage(0);
    if (!pPage)
      return false;

    CFX_RectF rtFDE = pPage->GetContentsBox();
    rtFDE.Offset(fOffSetX, fOffSetY);
    if (rtFDE.right() < rtEidt.right() && m_fScrollOffsetX > 0) {
      m_fScrollOffsetX += rtFDE.right() - rtEidt.right();
      m_fScrollOffsetX = std::max(m_fScrollOffsetX, 0.0f);
    }
    if (rtFDE.bottom() < rtEidt.bottom() && m_fScrollOffsetY > 0) {
      m_fScrollOffsetY += rtFDE.bottom() - rtEidt.bottom();
      m_fScrollOffsetY = std::max(m_fScrollOffsetY, 0.0f);
    }
    return false;
  }

  FX_FLOAT offsetX = 0.0;
  FX_FLOAT offsetY = 0.0;
  if (rtCaret.left < rtEidt.left)
    offsetX = rtCaret.left - rtEidt.left;
  if (rtCaret.right() > rtEidt.right())
    offsetX = rtCaret.right() - rtEidt.right();
  if (rtCaret.top < rtEidt.top)
    offsetY = rtCaret.top - rtEidt.top;
  if (rtCaret.bottom() > rtEidt.bottom())
    offsetY = rtCaret.bottom() - rtEidt.bottom();
  m_fScrollOffsetX += offsetX;
  m_fScrollOffsetY += offsetY;
  if (m_fFontSize > m_rtEngine.height)
    m_fScrollOffsetY = 0;
  return true;
}

bool IFWL_Edit::UpdateOffset(IFWL_ScrollBar* pScrollBar, FX_FLOAT fPosChanged) {
  if (pScrollBar == m_pHorzScrollBar.get())
    m_fScrollOffsetX += fPosChanged;
  else
    m_fScrollOffsetY += fPosChanged;
  return true;
}

void IFWL_Edit::UpdateVAlignment() {
  IFDE_TxtEdtPage* pPage = m_EdtEngine.GetPage(0);
  if (!pPage)
    return;

  const CFX_RectF& rtFDE = pPage->GetContentsBox();
  FX_FLOAT fOffsetY = 0.0f;
  FX_FLOAT fSpaceAbove = 0.0f;
  FX_FLOAT fSpaceBelow = 0.0f;
  CFX_SizeF* pSpace = static_cast<CFX_SizeF*>(
      GetThemeCapacity(CFWL_WidgetCapacity::SpaceAboveBelow));
  if (pSpace) {
    fSpaceAbove = pSpace->x;
    fSpaceBelow = pSpace->y;
  }
  if (fSpaceAbove < 0.1f)
    fSpaceAbove = 0;
  if (fSpaceBelow < 0.1f)
    fSpaceBelow = 0;

  if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_VCenter) {
    fOffsetY = (m_rtEngine.height - rtFDE.height) / 2;
    if (fOffsetY < (fSpaceAbove + fSpaceBelow) / 2 &&
        fSpaceAbove < fSpaceBelow) {
      return;
    }
    fOffsetY += (fSpaceAbove - fSpaceBelow) / 2;
  } else if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_VFar) {
    fOffsetY = (m_rtEngine.height - rtFDE.height);
    fOffsetY -= fSpaceBelow;
  } else {
    fOffsetY += fSpaceAbove;
  }
  m_fVAlignOffset = std::max(fOffsetY, 0.0f);
}

void IFWL_Edit::UpdateCaret() {
  CFX_RectF rtFDE;
  m_EdtEngine.GetCaretRect(rtFDE);

  rtFDE.Offset(m_rtEngine.left - m_fScrollOffsetX,
               m_rtEngine.top - m_fScrollOffsetY + m_fVAlignOffset);
  CFX_RectF rtCaret;
  rtCaret.Set(rtFDE.left, rtFDE.top, rtFDE.width, rtFDE.height);

  CFX_RectF rtClient;
  GetClientRect(rtClient);
  rtCaret.Intersect(rtClient);

  if (rtCaret.left > rtClient.right()) {
    FX_FLOAT right = rtCaret.right();
    rtCaret.left = rtClient.right() - 1;
    rtCaret.width = right - rtCaret.left;
  }

  bool bShow =
      m_pProperties->m_dwStates & FWL_WGTSTATE_Focused && !rtCaret.IsEmpty();
  ShowCaret(bShow, &rtCaret);
}

IFWL_ScrollBar* IFWL_Edit::UpdateScroll() {
  bool bShowHorz =
      m_pHorzScrollBar &&
      ((m_pHorzScrollBar->GetStates() & FWL_WGTSTATE_Invisible) == 0);
  bool bShowVert =
      m_pVertScrollBar &&
      ((m_pVertScrollBar->GetStates() & FWL_WGTSTATE_Invisible) == 0);
  if (!bShowHorz && !bShowVert)
    return nullptr;

  IFDE_TxtEdtPage* pPage = m_EdtEngine.GetPage(0);
  if (!pPage)
    return nullptr;

  const CFX_RectF& rtFDE = pPage->GetContentsBox();
  IFWL_ScrollBar* pRepaint = nullptr;
  if (bShowHorz) {
    CFX_RectF rtScroll;
    m_pHorzScrollBar->GetWidgetRect(rtScroll);
    if (rtScroll.width < rtFDE.width) {
      m_pHorzScrollBar->LockUpdate();
      FX_FLOAT fRange = rtFDE.width - rtScroll.width;
      m_pHorzScrollBar->SetRange(0.0f, fRange);

      FX_FLOAT fPos = std::min(std::max(m_fScrollOffsetX, 0.0f), fRange);
      m_pHorzScrollBar->SetPos(fPos);
      m_pHorzScrollBar->SetTrackPos(fPos);
      m_pHorzScrollBar->SetPageSize(rtScroll.width);
      m_pHorzScrollBar->SetStepSize(rtScroll.width / 10);
      m_pHorzScrollBar->SetStates(FWL_WGTSTATE_Disabled, false);
      m_pHorzScrollBar->UnlockUpdate();
      m_pHorzScrollBar->Update();
      pRepaint = m_pHorzScrollBar.get();
    } else if ((m_pHorzScrollBar->GetStates() & FWL_WGTSTATE_Disabled) == 0) {
      m_pHorzScrollBar->LockUpdate();
      m_pHorzScrollBar->SetRange(0, -1);
      m_pHorzScrollBar->SetStates(FWL_WGTSTATE_Disabled, true);
      m_pHorzScrollBar->UnlockUpdate();
      m_pHorzScrollBar->Update();
      pRepaint = m_pHorzScrollBar.get();
    }
  }

  if (bShowVert) {
    CFX_RectF rtScroll;
    m_pVertScrollBar->GetWidgetRect(rtScroll);
    if (rtScroll.height < rtFDE.height) {
      m_pVertScrollBar->LockUpdate();
      FX_FLOAT fStep = m_EdtEngine.GetEditParams()->fLineSpace;
      FX_FLOAT fRange = std::max(rtFDE.height - m_rtEngine.height, fStep);

      m_pVertScrollBar->SetRange(0.0f, fRange);
      FX_FLOAT fPos = std::min(std::max(m_fScrollOffsetY, 0.0f), fRange);
      m_pVertScrollBar->SetPos(fPos);
      m_pVertScrollBar->SetTrackPos(fPos);
      m_pVertScrollBar->SetPageSize(rtScroll.height);
      m_pVertScrollBar->SetStepSize(fStep);
      m_pVertScrollBar->SetStates(FWL_WGTSTATE_Disabled, false);
      m_pVertScrollBar->UnlockUpdate();
      m_pVertScrollBar->Update();
      pRepaint = m_pVertScrollBar.get();
    } else if ((m_pVertScrollBar->GetStates() & FWL_WGTSTATE_Disabled) == 0) {
      m_pVertScrollBar->LockUpdate();
      m_pVertScrollBar->SetRange(0, -1);
      m_pVertScrollBar->SetStates(FWL_WGTSTATE_Disabled, true);
      m_pVertScrollBar->UnlockUpdate();
      m_pVertScrollBar->Update();
      pRepaint = m_pVertScrollBar.get();
    }
  }
  return pRepaint;
}

bool IFWL_Edit::IsShowScrollBar(bool bVert) {
  bool bShow =
      (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_ShowScrollbarFocus)
          ? (m_pProperties->m_dwStates & FWL_WGTSTATE_Focused) ==
                FWL_WGTSTATE_Focused
          : true;
  if (bVert) {
    return bShow && (m_pProperties->m_dwStyles & FWL_WGTSTYLE_VScroll) &&
           (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_MultiLine) &&
           IsContentHeightOverflow();
  }
  return bShow && (m_pProperties->m_dwStyles & FWL_WGTSTYLE_HScroll) &&
         (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_MultiLine);
}

bool IFWL_Edit::IsContentHeightOverflow() {
  IFDE_TxtEdtPage* pPage = m_EdtEngine.GetPage(0);
  if (!pPage)
    return false;
  return pPage->GetContentsBox().height > m_rtEngine.height + 1.0f;
}

int32_t IFWL_Edit::AddDoRecord(IFDE_TxtEdtDoRecord* pRecord) {
  int32_t nCount = pdfium::CollectionSize<int32_t>(m_DoRecords);
  if (m_iCurRecord == nCount - 1) {
    if (nCount == m_iMaxRecord) {
      m_DoRecords.pop_front();
      m_iCurRecord--;
    }
  } else {
    m_DoRecords.erase(m_DoRecords.begin() + m_iCurRecord + 1,
                      m_DoRecords.end());
  }

  m_DoRecords.push_back(std::unique_ptr<IFDE_TxtEdtDoRecord>(pRecord));
  m_iCurRecord = pdfium::CollectionSize<int32_t>(m_DoRecords) - 1;
  return m_iCurRecord;
}

void IFWL_Edit::Layout() {
  GetClientRect(m_rtClient);
  m_rtEngine = m_rtClient;
  FX_FLOAT* pfWidth = static_cast<FX_FLOAT*>(
      GetThemeCapacity(CFWL_WidgetCapacity::ScrollBarWidth));
  if (!pfWidth)
    return;

  FX_FLOAT fWidth = *pfWidth;
  if (!m_pOuter) {
    CFX_RectF* pUIMargin = static_cast<CFX_RectF*>(
        GetThemeCapacity(CFWL_WidgetCapacity::UIMargin));
    if (pUIMargin) {
      m_rtEngine.Deflate(pUIMargin->left, pUIMargin->top, pUIMargin->width,
                         pUIMargin->height);
    }
  } else if (m_pOuter->GetClassID() == FWL_Type::DateTimePicker) {
    CFWL_ThemePart part;
    part.m_pWidget = m_pOuter;
    CFX_RectF* pUIMargin =
        static_cast<CFX_RectF*>(m_pOuter->GetThemeProvider()->GetCapacity(
            &part, CFWL_WidgetCapacity::UIMargin));
    if (pUIMargin) {
      m_rtEngine.Deflate(pUIMargin->left, pUIMargin->top, pUIMargin->width,
                         pUIMargin->height);
    }
  }

  bool bShowVertScrollbar = IsShowScrollBar(true);
  bool bShowHorzScrollbar = IsShowScrollBar(false);
  if (bShowVertScrollbar) {
    InitScrollBar();

    CFX_RectF rtVertScr;
    if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_OuterScrollbar) {
      rtVertScr.Set(m_rtClient.right() + kEditMargin, m_rtClient.top, fWidth,
                    m_rtClient.height);
    } else {
      rtVertScr.Set(m_rtClient.right() - fWidth, m_rtClient.top, fWidth,
                    m_rtClient.height);
      if (bShowHorzScrollbar)
        rtVertScr.height -= fWidth;
      m_rtEngine.width -= fWidth;
    }

    m_pVertScrollBar->SetWidgetRect(rtVertScr);
    m_pVertScrollBar->SetStates(FWL_WGTSTATE_Invisible, false);
    m_pVertScrollBar->Update();
  } else if (m_pVertScrollBar) {
    m_pVertScrollBar->SetStates(FWL_WGTSTATE_Invisible, true);
  }

  if (bShowHorzScrollbar) {
    InitScrollBar(false);

    CFX_RectF rtHoriScr;
    if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_OuterScrollbar) {
      rtHoriScr.Set(m_rtClient.left, m_rtClient.bottom() + kEditMargin,
                    m_rtClient.width, fWidth);
    } else {
      rtHoriScr.Set(m_rtClient.left, m_rtClient.bottom() - fWidth,
                    m_rtClient.width, fWidth);
      if (bShowVertScrollbar)
        rtHoriScr.width -= fWidth;
      m_rtEngine.height -= fWidth;
    }
    m_pHorzScrollBar->SetWidgetRect(rtHoriScr);
    m_pHorzScrollBar->SetStates(FWL_WGTSTATE_Invisible, false);
    m_pHorzScrollBar->Update();
  } else if (m_pHorzScrollBar) {
    m_pHorzScrollBar->SetStates(FWL_WGTSTATE_Invisible, true);
  }
}

void IFWL_Edit::LayoutScrollBar() {
  if ((m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_ShowScrollbarFocus) ==
      0) {
    return;
  }

  FX_FLOAT* pfWidth = nullptr;
  bool bShowVertScrollbar = IsShowScrollBar(true);
  bool bShowHorzScrollbar = IsShowScrollBar(false);
  if (bShowVertScrollbar) {
    if (!m_pVertScrollBar) {
      pfWidth = static_cast<FX_FLOAT*>(
          GetThemeCapacity(CFWL_WidgetCapacity::ScrollBarWidth));
      FX_FLOAT fWidth = pfWidth ? *pfWidth : 0;
      InitScrollBar();
      CFX_RectF rtVertScr;
      if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_OuterScrollbar) {
        rtVertScr.Set(m_rtClient.right() + kEditMargin, m_rtClient.top, fWidth,
                      m_rtClient.height);
      } else {
        rtVertScr.Set(m_rtClient.right() - fWidth, m_rtClient.top, fWidth,
                      m_rtClient.height);
        if (bShowHorzScrollbar)
          rtVertScr.height -= fWidth;
      }
      m_pVertScrollBar->SetWidgetRect(rtVertScr);
      m_pVertScrollBar->Update();
    }
    m_pVertScrollBar->SetStates(FWL_WGTSTATE_Invisible, false);
  } else if (m_pVertScrollBar) {
    m_pVertScrollBar->SetStates(FWL_WGTSTATE_Invisible, true);
  }

  if (bShowHorzScrollbar) {
    if (!m_pHorzScrollBar) {
      if (!pfWidth) {
        pfWidth = static_cast<FX_FLOAT*>(
            GetThemeCapacity(CFWL_WidgetCapacity::ScrollBarWidth));
      }

      FX_FLOAT fWidth = pfWidth ? *pfWidth : 0;
      InitScrollBar(false);
      CFX_RectF rtHoriScr;
      if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_OuterScrollbar) {
        rtHoriScr.Set(m_rtClient.left, m_rtClient.bottom() + kEditMargin,
                      m_rtClient.width, fWidth);
      } else {
        rtHoriScr.Set(m_rtClient.left, m_rtClient.bottom() - fWidth,
                      m_rtClient.width, fWidth);
        if (bShowVertScrollbar)
          rtHoriScr.width -= (fWidth);
      }
      m_pHorzScrollBar->SetWidgetRect(rtHoriScr);
      m_pHorzScrollBar->Update();
    }
    m_pHorzScrollBar->SetStates(FWL_WGTSTATE_Invisible, false);
  } else if (m_pHorzScrollBar) {
    m_pHorzScrollBar->SetStates(FWL_WGTSTATE_Invisible, true);
  }
  if (bShowVertScrollbar || bShowHorzScrollbar)
    UpdateScroll();
}

void IFWL_Edit::DeviceToEngine(CFX_PointF& pt) {
  pt.x += m_fScrollOffsetX - m_rtEngine.left;
  pt.y += m_fScrollOffsetY - m_rtEngine.top - m_fVAlignOffset;
}

void IFWL_Edit::InitScrollBar(bool bVert) {
  if ((bVert && m_pVertScrollBar) || (!bVert && m_pHorzScrollBar))
    return;

  auto prop = pdfium::MakeUnique<CFWL_WidgetProperties>();
  prop->m_dwStyleExes = bVert ? FWL_STYLEEXT_SCB_Vert : FWL_STYLEEXT_SCB_Horz;
  prop->m_dwStates = FWL_WGTSTATE_Disabled | FWL_WGTSTATE_Invisible;
  prop->m_pParent = this;
  prop->m_pThemeProvider = m_pProperties->m_pThemeProvider;

  IFWL_ScrollBar* sb = new IFWL_ScrollBar(m_pOwnerApp, std::move(prop), this);
  if (bVert)
    m_pVertScrollBar.reset(sb);
  else
    m_pHorzScrollBar.reset(sb);
}

bool FWL_ShowCaret(IFWL_Widget* pWidget,
                   bool bVisible,
                   const CFX_RectF* pRtAnchor) {
  CXFA_FFWidget* pXFAWidget =
      static_cast<CXFA_FFWidget*>(pWidget->GetLayoutItem());
  if (!pXFAWidget)
    return false;

  IXFA_DocEnvironment* pDocEnvironment =
      pXFAWidget->GetDoc()->GetDocEnvironment();
  if (!pDocEnvironment)
    return false;

  if (bVisible) {
    CFX_Matrix mt;
    pXFAWidget->GetRotateMatrix(mt);
    CFX_RectF rt(*pRtAnchor);
    mt.TransformRect(rt);
    pDocEnvironment->DisplayCaret(pXFAWidget, bVisible, &rt);
    return true;
  }

  pDocEnvironment->DisplayCaret(pXFAWidget, bVisible, pRtAnchor);
  return true;
}

void IFWL_Edit::ShowCaret(bool bVisible, CFX_RectF* pRect) {
  if (m_pCaret) {
    m_pCaret->ShowCaret(bVisible);
    if (bVisible && !pRect->IsEmpty())
      m_pCaret->SetWidgetRect(*pRect);
    Repaint(&m_rtEngine);
    return;
  }

  IFWL_Widget* pOuter = this;
  if (bVisible) {
    pRect->Offset(m_pProperties->m_rtWidget.left,
                  m_pProperties->m_rtWidget.top);
  }
  while (pOuter->GetOuter()) {
    pOuter = pOuter->GetOuter();
    if (bVisible) {
      CFX_RectF rtOuter;
      pOuter->GetWidgetRect(rtOuter);
      pRect->Offset(rtOuter.left, rtOuter.top);
    }
  }
  FWL_ShowCaret(pOuter, bVisible, pRect);
}

bool IFWL_Edit::ValidateNumberChar(FX_WCHAR cNum) {
  if (!m_bSetRange)
    return true;

  CFX_WideString wsOld, wsText;
  m_EdtEngine.GetText(wsText, 0);
  if (wsText.IsEmpty()) {
    if (cNum == L'0')
      return false;
    return true;
  }

  int32_t caretPos = m_EdtEngine.GetCaretPos();
  if (CountSelRanges() == 0) {
    if (cNum == L'0' && caretPos == 0)
      return false;

    int32_t nLen = wsText.GetLength();
    CFX_WideString l = wsText.Mid(0, caretPos);
    CFX_WideString r = wsText.Mid(caretPos, nLen - caretPos);
    CFX_WideString wsNew = l + cNum + r;
    if (wsNew.GetInteger() <= m_iMax)
      return true;
    return false;
  }

  if (wsText.GetInteger() <= m_iMax)
    return true;
  return false;
}

void IFWL_Edit::InitCaret() {
  if (!m_pCaret) {
    if ((m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_InnerCaret)) {
      m_pCaret.reset(new IFWL_Caret(
          m_pOwnerApp, pdfium::MakeUnique<CFWL_WidgetProperties>(), this));
      m_pCaret->SetParent(this);
      m_pCaret->SetStates(m_pProperties->m_dwStates);
    }
  } else if ((m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_InnerCaret) ==
             0) {
    m_pCaret.reset();
  }
}

void IFWL_Edit::ClearRecord() {
  m_iCurRecord = -1;
  m_DoRecords.clear();
}

void IFWL_Edit::ProcessInsertError(int32_t iError) {
  if (iError != -2)
    return;

  CFWL_EvtTextFull textFullEvent;
  textFullEvent.m_pSrcTarget = this;
  DispatchEvent(&textFullEvent);
}

void IFWL_Edit::OnProcessMessage(CFWL_Message* pMessage) {
  if (!pMessage)
    return;

  CFWL_MessageType dwMsgCode = pMessage->GetClassID();
  switch (dwMsgCode) {
    case CFWL_MessageType::SetFocus:
    case CFWL_MessageType::KillFocus:
      OnFocusChanged(pMessage, dwMsgCode == CFWL_MessageType::SetFocus);
      break;
    case CFWL_MessageType::Mouse: {
      CFWL_MsgMouse* pMsg = static_cast<CFWL_MsgMouse*>(pMessage);
      switch (pMsg->m_dwCmd) {
        case FWL_MouseCommand::LeftButtonDown:
          OnLButtonDown(pMsg);
          break;
        case FWL_MouseCommand::LeftButtonUp:
          OnLButtonUp(pMsg);
          break;
        case FWL_MouseCommand::LeftButtonDblClk:
          OnButtonDblClk(pMsg);
          break;
        case FWL_MouseCommand::Move:
          OnMouseMove(pMsg);
          break;
        case FWL_MouseCommand::RightButtonDown:
          DoButtonDown(pMsg);
          break;
        default:
          break;
      }
      break;
    }
    case CFWL_MessageType::Key: {
      CFWL_MsgKey* pKey = static_cast<CFWL_MsgKey*>(pMessage);
      if (pKey->m_dwCmd == FWL_KeyCommand::KeyDown)
        OnKeyDown(pKey);
      else if (pKey->m_dwCmd == FWL_KeyCommand::Char)
        OnChar(pKey);
      break;
    }
    default:
      break;
  }
  IFWL_Widget::OnProcessMessage(pMessage);
}

void IFWL_Edit::OnProcessEvent(CFWL_Event* pEvent) {
  if (!pEvent)
    return;
  if (pEvent->GetClassID() != CFWL_EventType::Scroll)
    return;

  IFWL_Widget* pSrcTarget = pEvent->m_pSrcTarget;
  if ((pSrcTarget == m_pVertScrollBar.get() && m_pVertScrollBar) ||
      (pSrcTarget == m_pHorzScrollBar.get() && m_pHorzScrollBar)) {
    CFWL_EvtScroll* pScrollEvent = static_cast<CFWL_EvtScroll*>(pEvent);
    OnScroll(static_cast<IFWL_ScrollBar*>(pSrcTarget),
             pScrollEvent->m_iScrollCode, pScrollEvent->m_fPos);
  }
}

void IFWL_Edit::OnDrawWidget(CFX_Graphics* pGraphics,
                             const CFX_Matrix* pMatrix) {
  DrawWidget(pGraphics, pMatrix);
}

void IFWL_Edit::DoButtonDown(CFWL_MsgMouse* pMsg) {
  if ((m_pProperties->m_dwStates & FWL_WGTSTATE_Focused) == 0)
    SetFocus(true);

  IFDE_TxtEdtPage* pPage = m_EdtEngine.GetPage(0);
  if (!pPage)
    return;

  CFX_PointF pt(pMsg->m_fx, pMsg->m_fy);
  DeviceToEngine(pt);
  bool bBefore = true;
  int32_t nIndex = pPage->GetCharIndex(pt, bBefore);
  if (nIndex < 0)
    nIndex = 0;

  m_EdtEngine.SetCaretPos(nIndex, bBefore);
}

void IFWL_Edit::OnFocusChanged(CFWL_Message* pMsg, bool bSet) {
  uint32_t dwStyleEx = GetStylesEx();
  bool bRepaint = !!(dwStyleEx & FWL_STYLEEXT_EDT_InnerCaret);
  if (bSet) {
    m_pProperties->m_dwStates |= FWL_WGTSTATE_Focused;

    UpdateVAlignment();
    UpdateOffset();
    UpdateCaret();
  } else if (m_pProperties->m_dwStates & FWL_WGTSTATE_Focused) {
    m_pProperties->m_dwStates &= ~FWL_WGTSTATE_Focused;
    ShowCaret(false);
    if ((dwStyleEx & FWL_STYLEEXT_EDT_NoHideSel) == 0) {
      int32_t nSel = CountSelRanges();
      if (nSel > 0) {
        ClearSelections();
        bRepaint = true;
      }
      m_EdtEngine.SetCaretPos(0, true);
      UpdateOffset();
    }
    ClearRecord();
  }

  LayoutScrollBar();
  if (!bRepaint)
    return;

  CFX_RectF rtInvalidate;
  rtInvalidate.Set(0, 0, m_pProperties->m_rtWidget.width,
                   m_pProperties->m_rtWidget.height);
  Repaint(&rtInvalidate);
}

void IFWL_Edit::OnLButtonDown(CFWL_MsgMouse* pMsg) {
  if (m_pProperties->m_dwStates & FWL_WGTSTATE_Disabled)
    return;

  m_bLButtonDown = true;
  SetGrab(true);
  DoButtonDown(pMsg);
  int32_t nIndex = m_EdtEngine.GetCaretPos();
  bool bRepaint = false;
  if (m_EdtEngine.CountSelRanges() > 0) {
    m_EdtEngine.ClearSelection();
    bRepaint = true;
  }

  if ((pMsg->m_dwFlags & FWL_KEYFLAG_Shift) && m_nSelStart != nIndex) {
    int32_t iStart = std::min(m_nSelStart, nIndex);
    int32_t iEnd = std::max(m_nSelStart, nIndex);
    m_EdtEngine.AddSelRange(iStart, iEnd - iStart);
    bRepaint = true;
  } else {
    m_nSelStart = nIndex;
  }
  if (bRepaint)
    Repaint(&m_rtEngine);
}

void IFWL_Edit::OnLButtonUp(CFWL_MsgMouse* pMsg) {
  m_bLButtonDown = false;
  SetGrab(false);
}

void IFWL_Edit::OnButtonDblClk(CFWL_MsgMouse* pMsg) {
  IFDE_TxtEdtPage* pPage = m_EdtEngine.GetPage(0);
  if (!pPage)
    return;

  CFX_PointF pt(pMsg->m_fx, pMsg->m_fy);
  DeviceToEngine(pt);
  int32_t nCount = 0;
  int32_t nIndex = pPage->SelectWord(pt, nCount);
  if (nIndex < 0)
    return;

  m_EdtEngine.AddSelRange(nIndex, nCount);
  m_EdtEngine.SetCaretPos(nIndex + nCount - 1, false);
  Repaint(&m_rtEngine);
}

void IFWL_Edit::OnMouseMove(CFWL_MsgMouse* pMsg) {
  if (m_nSelStart == -1 || !m_bLButtonDown)
    return;

  IFDE_TxtEdtPage* pPage = m_EdtEngine.GetPage(0);
  if (!pPage)
    return;

  CFX_PointF pt(pMsg->m_fx, pMsg->m_fy);
  DeviceToEngine(pt);
  bool bBefore = true;
  int32_t nIndex = pPage->GetCharIndex(pt, bBefore);
  m_EdtEngine.SetCaretPos(nIndex, bBefore);
  nIndex = m_EdtEngine.GetCaretPos();
  m_EdtEngine.ClearSelection();

  if (nIndex == m_nSelStart)
    return;

  int32_t nLen = m_EdtEngine.GetTextLength();
  if (m_nSelStart >= nLen)
    m_nSelStart = nLen;

  m_EdtEngine.AddSelRange(std::min(m_nSelStart, nIndex),
                          FXSYS_abs(nIndex - m_nSelStart));
}

void IFWL_Edit::OnKeyDown(CFWL_MsgKey* pMsg) {
  FDE_TXTEDTMOVECARET MoveCaret = MC_MoveNone;
  bool bShift = !!(pMsg->m_dwFlags & FWL_KEYFLAG_Shift);
  bool bCtrl = !!(pMsg->m_dwFlags & FWL_KEYFLAG_Ctrl);
  uint32_t dwKeyCode = pMsg->m_dwKeyCode;
  switch (dwKeyCode) {
    case FWL_VKEY_Left: {
      MoveCaret = MC_Left;
      break;
    }
    case FWL_VKEY_Right: {
      MoveCaret = MC_Right;
      break;
    }
    case FWL_VKEY_Up: {
      MoveCaret = MC_Up;
      break;
    }
    case FWL_VKEY_Down: {
      MoveCaret = MC_Down;
      break;
    }
    case FWL_VKEY_Home: {
      MoveCaret = bCtrl ? MC_Home : MC_LineStart;
      break;
    }
    case FWL_VKEY_End: {
      MoveCaret = bCtrl ? MC_End : MC_LineEnd;
      break;
    }
    case FWL_VKEY_Insert:
      break;
    case FWL_VKEY_Delete: {
      if ((m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_ReadOnly) ||
          (m_pProperties->m_dwStates & FWL_WGTSTATE_Disabled)) {
        break;
      }
      int32_t nCaret = m_EdtEngine.GetCaretPos();
#if (_FX_OS_ == _FX_MACOSX_)
      m_EdtEngine.Delete(nCaret, true);
#else
      m_EdtEngine.Delete(nCaret);
#endif
      break;
    }
    case FWL_VKEY_F2:
      break;
    case FWL_VKEY_Tab: {
      DispatchKeyEvent(pMsg);
      break;
    }
    default:
      break;
  }
  if (MoveCaret != MC_MoveNone)
    m_EdtEngine.MoveCaretPos(MoveCaret, bShift, bCtrl);
}

void IFWL_Edit::OnChar(CFWL_MsgKey* pMsg) {
  if ((m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_ReadOnly) ||
      (m_pProperties->m_dwStates & FWL_WGTSTATE_Disabled)) {
    return;
  }

  int32_t iError = 0;
  FX_WCHAR c = static_cast<FX_WCHAR>(pMsg->m_dwKeyCode);
  int32_t nCaret = m_EdtEngine.GetCaretPos();
  switch (c) {
    case FWL_VKEY_Back:
      m_EdtEngine.Delete(nCaret, true);
      break;
    case FWL_VKEY_NewLine:
    case FWL_VKEY_Escape:
      break;
    case FWL_VKEY_Tab: {
      iError = m_EdtEngine.Insert(nCaret, L"\t", 1);
      break;
    }
    case FWL_VKEY_Return: {
      if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_WantReturn) {
        iError = m_EdtEngine.Insert(nCaret, L"\n", 1);
      }
      break;
    }
    default: {
      if (!m_pWidgetMgr->IsFormDisabled()) {
        if (m_pProperties->m_dwStyleExes & FWL_STYLEEXT_EDT_Number) {
          if (((pMsg->m_dwKeyCode < FWL_VKEY_0) &&
               (pMsg->m_dwKeyCode != 0x2E && pMsg->m_dwKeyCode != 0x2D)) ||
              pMsg->m_dwKeyCode > FWL_VKEY_9) {
            break;
          }
          if (!ValidateNumberChar(c))
            break;
        }
      }
#if (_FX_OS_ == _FX_MACOSX_)
      if (pMsg->m_dwFlags & FWL_KEYFLAG_Command)
#else
      if (pMsg->m_dwFlags & FWL_KEYFLAG_Ctrl)
#endif
      {
        break;
      }
      iError = m_EdtEngine.Insert(nCaret, &c, 1);
      break;
    }
  }
  if (iError < 0)
    ProcessInsertError(iError);
}

bool IFWL_Edit::OnScroll(IFWL_ScrollBar* pScrollBar,
                         FWL_SCBCODE dwCode,
                         FX_FLOAT fPos) {
  CFX_SizeF fs;
  pScrollBar->GetRange(&fs.x, &fs.y);
  FX_FLOAT iCurPos = pScrollBar->GetPos();
  FX_FLOAT fStep = pScrollBar->GetStepSize();
  switch (dwCode) {
    case FWL_SCBCODE::Min: {
      fPos = fs.x;
      break;
    }
    case FWL_SCBCODE::Max: {
      fPos = fs.y;
      break;
    }
    case FWL_SCBCODE::StepBackward: {
      fPos -= fStep;
      if (fPos < fs.x + fStep / 2) {
        fPos = fs.x;
      }
      break;
    }
    case FWL_SCBCODE::StepForward: {
      fPos += fStep;
      if (fPos > fs.y - fStep / 2) {
        fPos = fs.y;
      }
      break;
    }
    case FWL_SCBCODE::PageBackward: {
      fPos -= pScrollBar->GetPageSize();
      if (fPos < fs.x) {
        fPos = fs.x;
      }
      break;
    }
    case FWL_SCBCODE::PageForward: {
      fPos += pScrollBar->GetPageSize();
      if (fPos > fs.y) {
        fPos = fs.y;
      }
      break;
    }
    case FWL_SCBCODE::Pos:
    case FWL_SCBCODE::TrackPos:
    case FWL_SCBCODE::None:
      break;
    case FWL_SCBCODE::EndScroll:
      return false;
  }
  if (iCurPos == fPos)
    return true;

  pScrollBar->SetPos(fPos);
  pScrollBar->SetTrackPos(fPos);
  UpdateOffset(pScrollBar, fPos - iCurPos);
  UpdateCaret();

  CFX_RectF rect;
  GetWidgetRect(rect);
  CFX_RectF rtInvalidate;
  rtInvalidate.Set(0, 0, rect.width + 2, rect.height + 2);
  Repaint(&rtInvalidate);
  return true;
}
