// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/theme/cfwl_widgettp.h"

#include <algorithm>

#include "xfa/fde/tto/fde_textout.h"
#include "xfa/fgas/font/fgas_gefont.h"
#include "xfa/fgas/font/cfgas_fontmgr.h"
#include "xfa/fwl/core/cfwl_themebackground.h"
#include "xfa/fwl/core/cfwl_themepart.h"
#include "xfa/fwl/core/cfwl_themetext.h"
#include "xfa/fwl/core/cfwl_widgetmgr.h"
#include "xfa/fwl/core/ifwl_themeprovider.h"
#include "xfa/fwl/core/ifwl_widget.h"
#include "xfa/fwl/theme/cfwl_arrowdata.h"
#include "xfa/fxgraphics/cfx_color.h"
#include "xfa/fxgraphics/cfx_path.h"
#include "xfa/fxgraphics/cfx_shading.h"

namespace {

const float kEdgeFlat = 2.0f;
const float kEdgeRaised = 2.0f;
const float kEdgeSunken = 2.0f;
const float kLineHeight = 12.0f;
const float kScrollBarWidth = 17.0f;
const float kCXBorder = 1.0f;
const float kCYBorder = 1.0f;

#define FWLTHEME_CAPACITY_TextSelColor (ArgbEncode(255, 153, 193, 218))

}  // namespace

bool CFWL_WidgetTP::IsValidWidget(IFWL_Widget* pWidget) {
  return false;
}

uint32_t CFWL_WidgetTP::GetThemeID(IFWL_Widget* pWidget) {
  return m_dwThemeID;
}

uint32_t CFWL_WidgetTP::SetThemeID(IFWL_Widget* pWidget, uint32_t dwThemeID) {
  uint32_t dwOld = m_dwThemeID;
  m_dwThemeID = dwThemeID;
  if (CFWL_ArrowData::HasInstance())
    CFWL_ArrowData::GetInstance()->SetColorData(FWL_GetThemeColor(dwThemeID));
  return dwOld;
}

void CFWL_WidgetTP::DrawBackground(CFWL_ThemeBackground* pParams) {}

void CFWL_WidgetTP::DrawText(CFWL_ThemeText* pParams) {
  if (!m_pTextOut)
    InitTTO();

  int32_t iLen = pParams->m_wsText.GetLength();
  if (iLen <= 0)
    return;

  CFX_Graphics* pGraphics = pParams->m_pGraphics;
  m_pTextOut->SetRenderDevice(pGraphics->GetRenderDevice());
  m_pTextOut->SetStyles(pParams->m_dwTTOStyles);
  m_pTextOut->SetAlignment(pParams->m_iTTOAlign);
  CFX_Matrix* pMatrix = &pParams->m_matrix;
  pMatrix->Concat(*pGraphics->GetMatrix());
  m_pTextOut->SetMatrix(*pMatrix);
  m_pTextOut->DrawLogicText(pParams->m_wsText.c_str(), iLen, pParams->m_rtPart);
}

void* CFWL_WidgetTP::GetCapacity(CFWL_ThemePart* pThemePart,
                                 CFWL_WidgetCapacity dwCapacity) {
  switch (dwCapacity) {
    case CFWL_WidgetCapacity::CXBorder: {
      m_fValue = kCXBorder;
      break;
    }
    case CFWL_WidgetCapacity::CYBorder: {
      m_fValue = kCYBorder;
      break;
    }
    case CFWL_WidgetCapacity::EdgeFlat: {
      m_fValue = kEdgeFlat;
      break;
    }
    case CFWL_WidgetCapacity::EdgeRaised: {
      m_fValue = kEdgeRaised;
      break;
    }
    case CFWL_WidgetCapacity::EdgeSunken: {
      m_fValue = kEdgeSunken;
      break;
    }
    case CFWL_WidgetCapacity::FontSize: {
      m_fValue = FWLTHEME_CAPACITY_FontSize;
      break;
    }
    case CFWL_WidgetCapacity::TextColor: {
      m_dwValue = FWLTHEME_CAPACITY_TextColor;
      return &m_dwValue;
    }
    case CFWL_WidgetCapacity::ScrollBarWidth: {
      m_fValue = kScrollBarWidth;
      break;
    }
    case CFWL_WidgetCapacity::Font: {
      return m_pFDEFont;
    }
    case CFWL_WidgetCapacity::TextSelColor: {
      m_dwValue = (m_dwThemeID == 0) ? FWLTHEME_CAPACITY_TextSelColor
                                     : FWLTHEME_COLOR_Green_BKSelected;
      return &m_dwValue;
    }
    case CFWL_WidgetCapacity::LineHeight: {
      m_fValue = kLineHeight;
      break;
    }
    case CFWL_WidgetCapacity::UIMargin: {
      m_rtMargin.Set(0, 0, 0, 0);
      return &m_rtMargin;
    }
    default:
      return nullptr;
  }
  return &m_fValue;
}

bool CFWL_WidgetTP::IsCustomizedLayout(IFWL_Widget* pWidget) {
  return !!FWL_GetThemeLayout(m_dwThemeID);
}

void CFWL_WidgetTP::CalcTextRect(CFWL_ThemeText* pParams, CFX_RectF& rect) {
  if (!pParams || !m_pTextOut)
    return;

  m_pTextOut->SetAlignment(pParams->m_iTTOAlign);
  m_pTextOut->SetStyles(pParams->m_dwTTOStyles | FDE_TTOSTYLE_ArabicContext);
  m_pTextOut->CalcLogicSize(pParams->m_wsText.c_str(),
                            pParams->m_wsText.GetLength(), rect);
}

void CFWL_WidgetTP::Initialize() {
  m_dwThemeID = 0;
}

void CFWL_WidgetTP::Finalize() {
  if (!m_pTextOut)
    FinalizeTTO();
}

CFWL_WidgetTP::~CFWL_WidgetTP() {}

void CFWL_WidgetTP::SetFont(IFWL_Widget* pWidget,
                            const FX_WCHAR* strFont,
                            FX_FLOAT fFontSize,
                            FX_ARGB rgbFont) {
  if (!m_pTextOut)
    return;

  m_pFDEFont = CFWL_FontManager::GetInstance()->FindFont(strFont, 0, 0);
  m_pTextOut->SetFont(m_pFDEFont);
  m_pTextOut->SetFontSize(fFontSize);
  m_pTextOut->SetTextColor(rgbFont);
}

void CFWL_WidgetTP::SetFont(IFWL_Widget* pWidget,
                            CFGAS_GEFont* pFont,
                            FX_FLOAT fFontSize,
                            FX_ARGB rgbFont) {
  if (!m_pTextOut)
    return;

  m_pTextOut->SetFont(pFont);
  m_pTextOut->SetFontSize(fFontSize);
  m_pTextOut->SetTextColor(rgbFont);
}

CFGAS_GEFont* CFWL_WidgetTP::GetFont(IFWL_Widget* pWidget) {
  return m_pFDEFont;
}

CFWL_WidgetTP::CFWL_WidgetTP()
    : m_dwRefCount(1), m_pFDEFont(nullptr), m_dwThemeID(0) {}

void CFWL_WidgetTP::InitTTO() {
  if (m_pTextOut)
    return;

  m_pFDEFont =
      CFWL_FontManager::GetInstance()->FindFont(FX_WSTRC(L"Helvetica"), 0, 0);
  m_pTextOut.reset(new CFDE_TextOut);
  m_pTextOut->SetFont(m_pFDEFont);
  m_pTextOut->SetFontSize(FWLTHEME_CAPACITY_FontSize);
  m_pTextOut->SetTextColor(FWLTHEME_CAPACITY_TextColor);
  m_pTextOut->SetEllipsisString(L"...");
}

void CFWL_WidgetTP::FinalizeTTO() {
  m_pTextOut.reset();
}

void CFWL_WidgetTP::DrawEdge(CFX_Graphics* pGraphics,
                             uint32_t dwStyles,
                             const CFX_RectF* pRect,
                             CFX_Matrix* pMatrix) {
  if (!pGraphics)
    return;
  if (!pRect)
    return;
  pGraphics->SaveGraphState();
  CFX_Color crStroke(FWL_GetThemeColor(m_dwThemeID) == 0
                         ? ArgbEncode(255, 127, 157, 185)
                         : FWLTHEME_COLOR_Green_BKSelected);
  pGraphics->SetStrokeColor(&crStroke);
  CFX_Path path;
  path.Create();
  path.AddRectangle(pRect->left, pRect->top, pRect->width - 1,
                    pRect->height - 1);
  pGraphics->StrokePath(&path, pMatrix);
  path.Clear();
  crStroke.Set(ArgbEncode(255, 255, 255, 255));
  pGraphics->SetStrokeColor(&crStroke);
  path.AddRectangle(pRect->left + 1, pRect->top + 1, pRect->width - 3,
                    pRect->height - 3);
  pGraphics->StrokePath(&path, pMatrix);
  pGraphics->RestoreGraphState();
}

void CFWL_WidgetTP::Draw3DRect(CFX_Graphics* pGraphics,
                               FWLTHEME_EDGE eType,
                               FX_FLOAT fWidth,
                               const CFX_RectF* pRect,
                               FX_ARGB cr1,
                               FX_ARGB cr2,
                               FX_ARGB cr3,
                               FX_ARGB cr4,
                               CFX_Matrix* pMatrix) {
  if (!pGraphics)
    return;
  if (!pRect)
    return;
  pGraphics->SaveGraphState();
  if (eType == FWLTHEME_EDGE_Flat) {
    CFX_Path path;
    path.Create();
    path.AddRectangle(pRect->left, pRect->top, pRect->width, pRect->height);
    path.AddRectangle(pRect->left + 1, pRect->top + 1, pRect->width - 2,
                      pRect->height - 2);
    CFX_Color cr(ArgbEncode(255, 100, 100, 100));
    pGraphics->SetFillColor(&cr);
    pGraphics->FillPath(&path, FXFILL_WINDING, pMatrix);
    path.Clear();
    path.AddRectangle(pRect->left + 1, pRect->top + 1, pRect->width - 2,
                      pRect->height - 2);
    path.AddRectangle(pRect->left + 2, pRect->top + 2, pRect->width - 4,
                      pRect->height - 4);
    cr.Set(0xFFFFFFFF);
    pGraphics->SetFillColor(&cr);
    pGraphics->FillPath(&path, FXFILL_WINDING, pMatrix);
  } else {
    FX_FLOAT fLeft = pRect->left;
    FX_FLOAT fRight = pRect->right();
    FX_FLOAT fTop = pRect->top;
    FX_FLOAT fBottom = pRect->bottom();
    FX_FLOAT fHalfWidth = fWidth / 2.0f;
    CFX_Color crLT(eType == FWLTHEME_EDGE_Raised ? cr4 : cr1);
    pGraphics->SetFillColor(&crLT);
    CFX_Path pathLT;
    pathLT.Create();
    pathLT.MoveTo(fLeft, fBottom - fHalfWidth);
    pathLT.LineTo(fLeft, fTop);
    pathLT.LineTo(fRight - fHalfWidth, fTop);
    pathLT.LineTo(fRight - fHalfWidth, fTop + fHalfWidth);
    pathLT.LineTo(fLeft + fHalfWidth, fTop + fHalfWidth);
    pathLT.LineTo(fLeft + fHalfWidth, fBottom - fHalfWidth);
    pathLT.LineTo(fLeft, fBottom - fHalfWidth);
    pGraphics->FillPath(&pathLT, FXFILL_WINDING, pMatrix);
    crLT = CFX_Color(eType == FWLTHEME_EDGE_Raised ? cr3 : cr2);
    pGraphics->SetFillColor(&crLT);
    pathLT.Clear();
    pathLT.MoveTo(fLeft + fHalfWidth, fBottom - fWidth);
    pathLT.LineTo(fLeft + fHalfWidth, fTop + fHalfWidth);
    pathLT.LineTo(fRight - fWidth, fTop + fHalfWidth);
    pathLT.LineTo(fRight - fWidth, fTop + fWidth);
    pathLT.LineTo(fLeft + fWidth, fTop + fWidth);
    pathLT.LineTo(fLeft + fWidth, fBottom - fWidth);
    pathLT.LineTo(fLeft + fHalfWidth, fBottom - fWidth);
    pGraphics->FillPath(&pathLT, FXFILL_WINDING, pMatrix);
    CFX_Color crRB(eType == FWLTHEME_EDGE_Raised ? cr1 : cr3);
    pGraphics->SetFillColor(&crRB);
    CFX_Path pathRB;
    pathRB.Create();
    pathRB.MoveTo(fRight - fHalfWidth, fTop + fHalfWidth);
    pathRB.LineTo(fRight - fHalfWidth, fBottom - fHalfWidth);
    pathRB.LineTo(fLeft + fHalfWidth, fBottom - fHalfWidth);
    pathRB.LineTo(fLeft + fHalfWidth, fBottom - fWidth);
    pathRB.LineTo(fRight - fWidth, fBottom - fWidth);
    pathRB.LineTo(fRight - fWidth, fTop + fHalfWidth);
    pathRB.LineTo(fRight - fHalfWidth, fTop + fHalfWidth);
    pGraphics->FillPath(&pathRB, FXFILL_WINDING, pMatrix);
    crRB = CFX_Color(eType == FWLTHEME_EDGE_Raised ? cr2 : cr4);
    pGraphics->SetFillColor(&crRB);
    pathRB.Clear();
    pathRB.MoveTo(fRight, fTop);
    pathRB.LineTo(fRight, fBottom);
    pathRB.LineTo(fLeft, fBottom);
    pathRB.LineTo(fLeft, fBottom - fHalfWidth);
    pathRB.LineTo(fRight - fHalfWidth, fBottom - fHalfWidth);
    pathRB.LineTo(fRight - fHalfWidth, fTop);
    pathRB.LineTo(fRight, fTop);
    pGraphics->FillPath(&pathRB, FXFILL_WINDING, pMatrix);
  }
  pGraphics->RestoreGraphState();
}

void CFWL_WidgetTP::Draw3DCircle(CFX_Graphics* pGraphics,
                                 FWLTHEME_EDGE eType,
                                 FX_FLOAT fWidth,
                                 const CFX_RectF* pRect,
                                 FX_ARGB cr1,
                                 FX_ARGB cr2,
                                 FX_ARGB cr3,
                                 FX_ARGB cr4,
                                 CFX_Matrix* pMatrix) {
  if (!pGraphics)
    return;
  if (!pRect)
    return;
  pGraphics->SaveGraphState();
  CFX_Path path;
  path.Create();
  path.AddArc(pRect->left, pRect->top, pRect->width, pRect->height,
              FX_PI * 3 / 4, FX_PI);
  CFX_Color crFill1(eType == FWLTHEME_EDGE_Raised ? cr4 : cr1);
  pGraphics->SetStrokeColor(&crFill1);
  pGraphics->StrokePath(&path, pMatrix);
  CFX_RectF rtInner(*pRect);
  rtInner.Deflate(pRect->width / 4, pRect->height / 4);
  path.Clear();
  path.AddArc(rtInner.left, rtInner.top, rtInner.width, rtInner.height,
              FX_PI * 3 / 4, FX_PI);
  CFX_Color crFill2(eType == FWLTHEME_EDGE_Raised ? cr3 : cr2);
  pGraphics->SetStrokeColor(&crFill2);
  pGraphics->StrokePath(&path, pMatrix);
  path.Clear();
  path.AddArc(pRect->left, pRect->top, pRect->width, pRect->height,
              FX_PI * 7 / 4, FX_PI);
  CFX_Color crFill3(eType == FWLTHEME_EDGE_Raised ? cr1 : cr3);
  pGraphics->SetStrokeColor(&crFill3);
  pGraphics->StrokePath(&path, pMatrix);
  path.AddArc(rtInner.left, rtInner.top, rtInner.width, rtInner.height,
              FX_PI * 7 / 4, FX_PI);
  CFX_Color crFill4(eType == FWLTHEME_EDGE_Raised ? cr2 : cr4);
  pGraphics->SetStrokeColor(&crFill4);
  pGraphics->StrokePath(&path, pMatrix);
  pGraphics->RestoreGraphState();
}

void CFWL_WidgetTP::DrawBorder(CFX_Graphics* pGraphics,
                               const CFX_RectF* pRect,
                               CFX_Matrix* pMatrix) {
  if (!pGraphics)
    return;
  if (!pRect)
    return;
  CFX_Path path;
  path.Create();
  path.AddRectangle(pRect->left, pRect->top, pRect->width, pRect->height);
  path.AddRectangle(pRect->left + 1, pRect->top + 1, pRect->width - 2,
                    pRect->height - 2);
  pGraphics->SaveGraphState();
  CFX_Color crFill(ArgbEncode(255, 0, 0, 0));
  pGraphics->SetFillColor(&crFill);
  pGraphics->FillPath(&path, FXFILL_ALTERNATE, pMatrix);
  pGraphics->RestoreGraphState();
}

void CFWL_WidgetTP::FillBackground(CFX_Graphics* pGraphics,
                                   const CFX_RectF* pRect,
                                   CFX_Matrix* pMatrix) {
  FillSoildRect(pGraphics, FWLTHEME_COLOR_Background, pRect, pMatrix);
}

void CFWL_WidgetTP::FillSoildRect(CFX_Graphics* pGraphics,
                                  FX_ARGB fillColor,
                                  const CFX_RectF* pRect,
                                  CFX_Matrix* pMatrix) {
  if (!pGraphics)
    return;
  if (!pRect)
    return;
  pGraphics->SaveGraphState();
  CFX_Color crFill(fillColor);
  pGraphics->SetFillColor(&crFill);
  CFX_Path path;
  path.Create();
  path.AddRectangle(pRect->left, pRect->top, pRect->width, pRect->height);
  pGraphics->FillPath(&path, FXFILL_WINDING, pMatrix);
  pGraphics->RestoreGraphState();
}

void CFWL_WidgetTP::DrawAxialShading(CFX_Graphics* pGraphics,
                                     FX_FLOAT fx1,
                                     FX_FLOAT fy1,
                                     FX_FLOAT fx2,
                                     FX_FLOAT fy2,
                                     FX_ARGB beginColor,
                                     FX_ARGB endColor,
                                     CFX_Path* path,
                                     int32_t fillMode,
                                     CFX_Matrix* pMatrix) {
  if (!pGraphics || !path)
    return;

  CFX_PointF begPoint(fx1, fy1);
  CFX_PointF endPoint(fx2, fy2);
  CFX_Shading shading(begPoint, endPoint, false, false, beginColor, endColor);
  pGraphics->SaveGraphState();
  CFX_Color color1(&shading);
  pGraphics->SetFillColor(&color1);
  pGraphics->FillPath(path, fillMode, pMatrix);
  pGraphics->RestoreGraphState();
}

void CFWL_WidgetTP::DrawAnnulusRect(CFX_Graphics* pGraphics,
                                    FX_ARGB fillColor,
                                    const CFX_RectF* pRect,
                                    FX_FLOAT fRingWidth,
                                    CFX_Matrix* pMatrix) {
  if (!pGraphics)
    return;
  if (!pRect)
    return;
  pGraphics->SaveGraphState();
  CFX_Color cr(fillColor);
  pGraphics->SetFillColor(&cr);
  CFX_Path path;
  path.Create();
  CFX_RectF rtInner(*pRect);
  rtInner.Deflate(fRingWidth, fRingWidth);
  path.AddRectangle(rtInner.left, rtInner.top, rtInner.width, rtInner.height);
  path.AddRectangle(pRect->left, pRect->top, pRect->width, pRect->height);
  pGraphics->FillPath(&path, FXFILL_ALTERNATE, pMatrix);
  pGraphics->RestoreGraphState();
}

void CFWL_WidgetTP::DrawAnnulusCircle(CFX_Graphics* pGraphics,
                                      FX_ARGB fillColor,
                                      const CFX_RectF* pRect,
                                      FX_FLOAT fWidth,
                                      CFX_Matrix* pMatrix) {
  if (!pGraphics)
    return;
  if (!pRect)
    return;
  if (fWidth > pRect->width / 2) {
    return;
  }
  pGraphics->SaveGraphState();
  CFX_Color cr(fillColor);
  pGraphics->SetFillColor(&cr);
  CFX_Path path;
  path.Create();
  path.AddEllipse(*pRect);
  CFX_RectF rtIn(*pRect);
  rtIn.Inflate(-fWidth, -fWidth);
  path.AddEllipse(rtIn);
  pGraphics->FillPath(&path, FXFILL_ALTERNATE, pMatrix);
  pGraphics->RestoreGraphState();
}

void CFWL_WidgetTP::DrawFocus(CFX_Graphics* pGraphics,
                              const CFX_RectF* pRect,
                              CFX_Matrix* pMatrix) {
  if (!pGraphics)
    return;
  if (!pRect)
    return;
  pGraphics->SaveGraphState();
  CFX_Color cr(0xFF000000);
  pGraphics->SetStrokeColor(&cr);
  FX_FLOAT DashPattern[2] = {1, 1};
  pGraphics->SetLineDash(0.0f, DashPattern, 2);
  CFX_Path path;
  path.Create();
  path.AddRectangle(pRect->left, pRect->top, pRect->width, pRect->height);
  pGraphics->StrokePath(&path, pMatrix);
  pGraphics->RestoreGraphState();
}
#define FWLTHEME_ARROW_Denominator 3
void CFWL_WidgetTP::DrawArrow(CFX_Graphics* pGraphics,
                              const CFX_RectF* pRect,
                              FWLTHEME_DIRECTION eDict,
                              FX_ARGB argbFill,
                              bool bPressed,
                              CFX_Matrix* pMatrix) {
  CFX_RectF rtArrow(*pRect);
  CFX_Path path;
  path.Create();
  FX_FLOAT fBtn =
      std::min(pRect->width, pRect->height) / FWLTHEME_ARROW_Denominator;
  rtArrow.left = pRect->left + (pRect->width - fBtn) / 2;
  rtArrow.top = pRect->top + (pRect->height - fBtn) / 2;
  rtArrow.width = fBtn;
  rtArrow.height = fBtn;
  if (bPressed) {
    rtArrow.Offset(1, 1);
  }
  switch (eDict) {
    case FWLTHEME_DIRECTION_Up: {
      path.MoveTo(rtArrow.left, rtArrow.bottom());
      path.LineTo(rtArrow.right(), rtArrow.bottom());
      path.LineTo(rtArrow.left + fBtn / 2, rtArrow.top);
      path.LineTo(rtArrow.left, rtArrow.bottom());
      break;
    }
    case FWLTHEME_DIRECTION_Left: {
      path.MoveTo(rtArrow.right(), rtArrow.top);
      path.LineTo(rtArrow.right(), rtArrow.bottom());
      path.LineTo(rtArrow.left, rtArrow.top + fBtn / 2);
      path.LineTo(rtArrow.right(), rtArrow.top);
      break;
    }
    case FWLTHEME_DIRECTION_Right: {
      path.MoveTo(rtArrow.left, rtArrow.top);
      path.LineTo(rtArrow.left, rtArrow.bottom());
      path.LineTo(rtArrow.right(), rtArrow.top + fBtn / 2);
      path.LineTo(rtArrow.left, rtArrow.top);
      break;
    }
    case FWLTHEME_DIRECTION_Down:
    default: {
      path.MoveTo(rtArrow.left, rtArrow.top);
      path.LineTo(rtArrow.right(), rtArrow.top);
      path.LineTo(rtArrow.left + fBtn / 2, rtArrow.bottom());
      path.LineTo(rtArrow.left, rtArrow.top);
    }
  }
  pGraphics->SaveGraphState();
  CFX_Color cr(argbFill);
  pGraphics->SetFillColor(&cr);
  pGraphics->FillPath(&path, FXFILL_WINDING, pMatrix);
  pGraphics->RestoreGraphState();
}

void CFWL_WidgetTP::DrawArrow(CFX_Graphics* pGraphics,
                              const CFX_RectF* pRect,
                              FWLTHEME_DIRECTION eDict,
                              FX_ARGB argSign,
                              CFX_Matrix* pMatrix) {
  bool bVert =
      (eDict == FWLTHEME_DIRECTION_Up || eDict == FWLTHEME_DIRECTION_Down);
  FX_FLOAT fLeft =
      (FX_FLOAT)(((pRect->width - (bVert ? 9 : 6)) / 2 + pRect->left) + 0.5);
  FX_FLOAT fTop =
      (FX_FLOAT)(((pRect->height - (bVert ? 6 : 9)) / 2 + pRect->top) + 0.5);
  CFX_Path path;
  path.Create();
  switch (eDict) {
    case FWLTHEME_DIRECTION_Down: {
      path.MoveTo(fLeft, fTop + 1);
      path.LineTo(fLeft + 4, fTop + 5);
      path.LineTo(fLeft + 8, fTop + 1);
      path.LineTo(fLeft + 7, fTop);
      path.LineTo(fLeft + 4, fTop + 3);
      path.LineTo(fLeft + 1, fTop);
      break;
    }
    case FWLTHEME_DIRECTION_Up: {
      path.MoveTo(fLeft, fTop + 4);
      path.LineTo(fLeft + 4, fTop);
      path.LineTo(fLeft + 8, fTop + 4);
      path.LineTo(fLeft + 7, fTop + 5);
      path.LineTo(fLeft + 4, fTop + 2);
      path.LineTo(fLeft + 1, fTop + 5);
      break;
    }
    case FWLTHEME_DIRECTION_Right: {
      path.MoveTo(fLeft + 1, fTop);
      path.LineTo(fLeft + 5, fTop + 4);
      path.LineTo(fLeft + 1, fTop + 8);
      path.LineTo(fLeft, fTop + 7);
      path.LineTo(fLeft + 3, fTop + 4);
      path.LineTo(fLeft, fTop + 1);
      break;
    }
    case FWLTHEME_DIRECTION_Left: {
      path.MoveTo(fLeft, fTop + 4);
      path.LineTo(fLeft + 4, fTop);
      path.LineTo(fLeft + 5, fTop + 1);
      path.LineTo(fLeft + 2, fTop + 4);
      path.LineTo(fLeft + 5, fTop + 7);
      path.LineTo(fLeft + 4, fTop + 8);
      break;
    }
  }
  CFX_Color cr(argSign);
  pGraphics->SetFillColor(&cr);
  pGraphics->FillPath(&path, FXFILL_WINDING, pMatrix);
}

void CFWL_WidgetTP::DrawBtn(CFX_Graphics* pGraphics,
                            const CFX_RectF* pRect,
                            FWLTHEME_STATE eState,
                            CFX_Matrix* pMatrix) {
  CFX_Path path;
  path.Create();
  if (!CFWL_ArrowData::HasInstance()) {
    CFWL_ArrowData::GetInstance()->SetColorData(FWL_GetThemeColor(m_dwThemeID));
  }
  CFWL_ArrowData::CColorData* pColorData =
      CFWL_ArrowData::GetInstance()->m_pColorData.get();
  FX_FLOAT fRight = pRect->right();
  FX_FLOAT fBottom = pRect->bottom();
  path.AddRectangle(pRect->left, pRect->top, pRect->width, pRect->height);
  DrawAxialShading(pGraphics, pRect->left, pRect->top, fRight, fBottom,
                   pColorData->clrStart[eState - 1],
                   pColorData->clrEnd[eState - 1], &path, FXFILL_WINDING,
                   pMatrix);
  CFX_Color rcStroke;
  rcStroke.Set(pColorData->clrBorder[eState - 1]);
  pGraphics->SetStrokeColor(&rcStroke);
  pGraphics->StrokePath(&path, pMatrix);
}

void CFWL_WidgetTP::DrawArrowBtn(CFX_Graphics* pGraphics,
                                 const CFX_RectF* pRect,
                                 FWLTHEME_DIRECTION eDict,
                                 FWLTHEME_STATE eState,
                                 CFX_Matrix* pMatrix) {
  DrawBtn(pGraphics, pRect, eState, pMatrix);
  if (!CFWL_ArrowData::HasInstance()) {
    CFWL_ArrowData::GetInstance()->SetColorData(FWL_GetThemeColor(m_dwThemeID));
  }
  CFWL_ArrowData::CColorData* pColorData =
      CFWL_ArrowData::GetInstance()->m_pColorData.get();
  DrawArrow(pGraphics, pRect, eDict, pColorData->clrSign[eState - 1], pMatrix);
}

CFWL_ArrowData::CFWL_ArrowData() : m_pColorData(nullptr) {
  SetColorData(0);
}

CFWL_FontData::CFWL_FontData() : m_dwStyles(0), m_dwCodePage(0) {}

CFWL_FontData::~CFWL_FontData() {}

bool CFWL_FontData::Equal(const CFX_WideStringC& wsFontFamily,
                          uint32_t dwFontStyles,
                          uint16_t wCodePage) {
  return m_wsFamily == wsFontFamily && m_dwStyles == dwFontStyles &&
         m_dwCodePage == wCodePage;
}

bool CFWL_FontData::LoadFont(const CFX_WideStringC& wsFontFamily,
                             uint32_t dwFontStyles,
                             uint16_t dwCodePage) {
  m_wsFamily = wsFontFamily;
  m_dwStyles = dwFontStyles;
  m_dwCodePage = dwCodePage;
  if (!m_pFontMgr) {
#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
    m_pFontMgr = CFGAS_FontMgr::Create(FX_GetDefFontEnumerator());
#else
    m_pFontSource.reset(new CFX_FontSourceEnum_File);
    m_pFontMgr = CFGAS_FontMgr::Create(m_pFontSource.get());
#endif
  }
  m_pFont.reset(CFGAS_GEFont::LoadFont(wsFontFamily.c_str(), dwFontStyles,
                                       dwCodePage, m_pFontMgr.get()));
  return !!m_pFont;
}

CFWL_FontManager* CFWL_FontManager::s_FontManager = nullptr;
CFWL_FontManager* CFWL_FontManager::GetInstance() {
  if (!s_FontManager)
    s_FontManager = new CFWL_FontManager;
  return s_FontManager;
}

void CFWL_FontManager::DestroyInstance() {
  delete s_FontManager;
  s_FontManager = nullptr;
}

CFWL_FontManager::CFWL_FontManager() {}

CFWL_FontManager::~CFWL_FontManager() {}

CFGAS_GEFont* CFWL_FontManager::FindFont(const CFX_WideStringC& wsFontFamily,
                                         uint32_t dwFontStyles,
                                         uint16_t wCodePage) {
  for (const auto& pData : m_FontsArray) {
    if (pData->Equal(wsFontFamily, dwFontStyles, wCodePage))
      return pData->GetFont();
  }
  std::unique_ptr<CFWL_FontData> pFontData(new CFWL_FontData);
  if (!pFontData->LoadFont(wsFontFamily, dwFontStyles, wCodePage))
    return nullptr;
  m_FontsArray.push_back(std::move(pFontData));
  return m_FontsArray.back()->GetFont();
}

void FWLTHEME_Release() {
  CFWL_ArrowData::DestroyInstance();
  CFWL_FontManager::DestroyInstance();
}

uint32_t FWL_GetThemeLayout(uint32_t dwThemeID) {
  return 0xffff0000 & dwThemeID;
}

uint32_t FWL_GetThemeColor(uint32_t dwThemeID) {
  return 0x0000ffff & dwThemeID;
}
