// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/render/render_int.h"

#include <memory>

#include "core/fpdfapi/cpdf_modulemgr.h"
#include "core/fpdfapi/font/cpdf_type3char.h"
#include "core/fpdfapi/font/cpdf_type3font.h"
#include "core/fpdfapi/page/cpdf_docpagedata.h"
#include "core/fpdfapi/page/cpdf_form.h"
#include "core/fpdfapi/page/cpdf_formobject.h"
#include "core/fpdfapi/page/cpdf_graphicstates.h"
#include "core/fpdfapi/page/cpdf_image.h"
#include "core/fpdfapi/page/cpdf_imageobject.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fpdfapi/page/cpdf_pathobject.h"
#include "core/fpdfapi/page/cpdf_textobject.h"
#include "core/fpdfapi/page/pageint.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/render/cpdf_docrenderdata.h"
#include "core/fpdfapi/render/cpdf_pagerendercache.h"
#include "core/fpdfapi/render/cpdf_progressiverenderer.h"
#include "core/fpdfapi/render/cpdf_renderoptions.h"
#include "core/fpdfapi/render/cpdf_textrenderer.h"
#include "core/fpdfapi/render/cpdf_type3cache.h"
#include "core/fpdfdoc/cpdf_occontext.h"
#include "core/fxge/cfx_fxgedevice.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"

CPDF_RenderOptions::CPDF_RenderOptions()
    : m_ColorMode(RENDER_COLOR_NORMAL),
      m_Flags(RENDER_CLEARTYPE),
      m_Interpolation(0),
      m_AddFlags(0),
      m_pOCContext(nullptr),
      m_dwLimitCacheSize(1024 * 1024 * 100),
      m_HalftoneLimit(-1),
      m_bDrawAnnots(false) {}

CPDF_RenderOptions::CPDF_RenderOptions(const CPDF_RenderOptions& rhs)
    : m_ColorMode(rhs.m_ColorMode),
      m_BackColor(rhs.m_BackColor),
      m_ForeColor(rhs.m_ForeColor),
      m_Flags(rhs.m_Flags),
      m_Interpolation(rhs.m_Interpolation),
      m_AddFlags(rhs.m_AddFlags),
      m_pOCContext(rhs.m_pOCContext),
      m_dwLimitCacheSize(rhs.m_dwLimitCacheSize),
      m_HalftoneLimit(rhs.m_HalftoneLimit),
      m_bDrawAnnots(rhs.m_bDrawAnnots) {}

FX_ARGB CPDF_RenderOptions::TranslateColor(FX_ARGB argb) const {
  if (m_ColorMode == RENDER_COLOR_NORMAL) {
    return argb;
  }
  if (m_ColorMode == RENDER_COLOR_ALPHA) {
    return argb;
  }
  int a, r, g, b;
  ArgbDecode(argb, a, r, g, b);
  int gray = FXRGB2GRAY(r, g, b);
  if (m_ColorMode == RENDER_COLOR_TWOCOLOR) {
    int color = (r - gray) * (r - gray) + (g - gray) * (g - gray) +
                (b - gray) * (b - gray);
    if (gray < 35 && color < 20) {
      return ArgbEncode(a, m_ForeColor);
    }
    if (gray > 221 && color < 20) {
      return ArgbEncode(a, m_BackColor);
    }
    return argb;
  }
  int fr = FXSYS_GetRValue(m_ForeColor);
  int fg = FXSYS_GetGValue(m_ForeColor);
  int fb = FXSYS_GetBValue(m_ForeColor);
  int br = FXSYS_GetRValue(m_BackColor);
  int bg = FXSYS_GetGValue(m_BackColor);
  int bb = FXSYS_GetBValue(m_BackColor);
  r = (br - fr) * gray / 255 + fr;
  g = (bg - fg) * gray / 255 + fg;
  b = (bb - fb) * gray / 255 + fb;
  return ArgbEncode(a, r, g, b);
}

// static
int CPDF_RenderStatus::s_CurrentRecursionDepth = 0;

CPDF_RenderStatus::CPDF_RenderStatus()
    : m_pFormResource(nullptr),
      m_pPageResource(nullptr),
      m_pContext(nullptr),
      m_bStopped(false),
      m_pDevice(nullptr),
      m_pCurObj(nullptr),
      m_pStopObj(nullptr),
      m_HalftoneLimit(0),
      m_bPrint(false),
      m_Transparency(0),
      m_bDropObjects(false),
      m_bStdCS(false),
      m_GroupFamily(0),
      m_bLoadMask(false),
      m_pType3Char(nullptr),
      m_T3FillColor(0),
      m_curBlend(FXDIB_BLEND_NORMAL) {}

CPDF_RenderStatus::~CPDF_RenderStatus() {}

bool CPDF_RenderStatus::Initialize(CPDF_RenderContext* pContext,
                                   CFX_RenderDevice* pDevice,
                                   const CFX_Matrix* pDeviceMatrix,
                                   const CPDF_PageObject* pStopObj,
                                   const CPDF_RenderStatus* pParentState,
                                   const CPDF_GraphicStates* pInitialStates,
                                   const CPDF_RenderOptions* pOptions,
                                   int transparency,
                                   bool bDropObjects,
                                   CPDF_Dictionary* pFormResource,
                                   bool bStdCS,
                                   CPDF_Type3Char* pType3Char,
                                   FX_ARGB fill_color,
                                   uint32_t GroupFamily,
                                   bool bLoadMask) {
  m_pContext = pContext;
  m_pDevice = pDevice;
  m_bPrint = m_pDevice->GetDeviceClass() != FXDC_DISPLAY;
  if (pDeviceMatrix) {
    m_DeviceMatrix = *pDeviceMatrix;
  }
  m_pStopObj = pStopObj;
  if (pOptions) {
    m_Options = *pOptions;
  }
  m_bDropObjects = bDropObjects;
  m_bStdCS = bStdCS;
  m_T3FillColor = fill_color;
  m_pType3Char = pType3Char;
  m_GroupFamily = GroupFamily;
  m_bLoadMask = bLoadMask;
  m_pFormResource = pFormResource;
  m_pPageResource = m_pContext->GetPageResources();
  if (pInitialStates && !m_pType3Char) {
    m_InitialStates.CopyStates(*pInitialStates);
    if (pParentState) {
      if (!m_InitialStates.m_ColorState.HasFillColor()) {
        m_InitialStates.m_ColorState.SetFillRGB(
            pParentState->m_InitialStates.m_ColorState.GetFillRGB());
        m_InitialStates.m_ColorState.GetMutableFillColor()->Copy(
            pParentState->m_InitialStates.m_ColorState.GetFillColor());
      }
      if (!m_InitialStates.m_ColorState.HasStrokeColor()) {
        m_InitialStates.m_ColorState.SetStrokeRGB(
            pParentState->m_InitialStates.m_ColorState.GetFillRGB());
        m_InitialStates.m_ColorState.GetMutableStrokeColor()->Copy(
            pParentState->m_InitialStates.m_ColorState.GetStrokeColor());
      }
    }
  } else {
    m_InitialStates.DefaultStates();
  }
  m_pImageRenderer.reset();
  m_Transparency = transparency;
  return true;
}
void CPDF_RenderStatus::RenderObjectList(
    const CPDF_PageObjectHolder* pObjectHolder,
    const CFX_Matrix* pObj2Device) {
#if defined _SKIA_SUPPORT_
  DebugVerifyDeviceIsPreMultiplied();
#endif
  CFX_FloatRect clip_rect(m_pDevice->GetClipBox());
  CFX_Matrix device2object;
  device2object.SetReverse(*pObj2Device);
  device2object.TransformRect(clip_rect);

  for (const auto& pCurObj : *pObjectHolder->GetPageObjectList()) {
    if (pCurObj.get() == m_pStopObj) {
      m_bStopped = true;
      return;
    }
    if (!pCurObj)
      continue;

    if (pCurObj->m_Left > clip_rect.right ||
        pCurObj->m_Right < clip_rect.left ||
        pCurObj->m_Bottom > clip_rect.top ||
        pCurObj->m_Top < clip_rect.bottom) {
      continue;
    }
    RenderSingleObject(pCurObj.get(), pObj2Device);
    if (m_bStopped)
      return;
  }
#if defined _SKIA_SUPPORT_
  DebugVerifyDeviceIsPreMultiplied();
#endif
}

void CPDF_RenderStatus::RenderSingleObject(CPDF_PageObject* pObj,
                                           const CFX_Matrix* pObj2Device) {
#if defined _SKIA_SUPPORT_
  DebugVerifyDeviceIsPreMultiplied();
#endif
  CFX_AutoRestorer<int> restorer(&s_CurrentRecursionDepth);
  if (++s_CurrentRecursionDepth > kRenderMaxRecursionDepth) {
    return;
  }
  m_pCurObj = pObj;
  if (m_Options.m_pOCContext && pObj->m_ContentMark) {
    if (!m_Options.m_pOCContext->CheckObjectVisible(pObj)) {
      return;
    }
  }
  ProcessClipPath(pObj->m_ClipPath, pObj2Device);
  if (ProcessTransparency(pObj, pObj2Device)) {
    return;
  }
  ProcessObjectNoClip(pObj, pObj2Device);
#if defined _SKIA_SUPPORT_
  DebugVerifyDeviceIsPreMultiplied();
#endif
}

bool CPDF_RenderStatus::ContinueSingleObject(CPDF_PageObject* pObj,
                                             const CFX_Matrix* pObj2Device,
                                             IFX_Pause* pPause) {
  if (m_pImageRenderer) {
    if (m_pImageRenderer->Continue(pPause))
      return true;

    if (!m_pImageRenderer->m_Result)
      DrawObjWithBackground(pObj, pObj2Device);
    m_pImageRenderer.reset();
    return false;
  }

  m_pCurObj = pObj;
  if (m_Options.m_pOCContext && pObj->m_ContentMark &&
      !m_Options.m_pOCContext->CheckObjectVisible(pObj)) {
    return false;
  }

  ProcessClipPath(pObj->m_ClipPath, pObj2Device);
  if (ProcessTransparency(pObj, pObj2Device))
    return false;

  if (pObj->IsImage()) {
    m_pImageRenderer.reset(new CPDF_ImageRenderer);
    if (!m_pImageRenderer->Start(this, pObj, pObj2Device, false)) {
      if (!m_pImageRenderer->m_Result)
        DrawObjWithBackground(pObj, pObj2Device);
      m_pImageRenderer.reset();
      return false;
    }
    return ContinueSingleObject(pObj, pObj2Device, pPause);
  }

  ProcessObjectNoClip(pObj, pObj2Device);
  return false;
}

bool CPDF_RenderStatus::GetObjectClippedRect(const CPDF_PageObject* pObj,
                                             const CFX_Matrix* pObj2Device,
                                             bool bLogical,
                                             FX_RECT& rect) const {
  rect = pObj->GetBBox(pObj2Device);
  FX_RECT rtClip = m_pDevice->GetClipBox();
  if (!bLogical) {
    CFX_Matrix dCTM = m_pDevice->GetCTM();
    FX_FLOAT a = FXSYS_fabs(dCTM.a);
    FX_FLOAT d = FXSYS_fabs(dCTM.d);
    if (a != 1.0f || d != 1.0f) {
      rect.right = rect.left + (int32_t)FXSYS_ceil((FX_FLOAT)rect.Width() * a);
      rect.bottom = rect.top + (int32_t)FXSYS_ceil((FX_FLOAT)rect.Height() * d);
      rtClip.right =
          rtClip.left + (int32_t)FXSYS_ceil((FX_FLOAT)rtClip.Width() * a);
      rtClip.bottom =
          rtClip.top + (int32_t)FXSYS_ceil((FX_FLOAT)rtClip.Height() * d);
    }
  }
  rect.Intersect(rtClip);
  return rect.IsEmpty();
}

void CPDF_RenderStatus::ProcessObjectNoClip(CPDF_PageObject* pObj,
                                            const CFX_Matrix* pObj2Device) {
#if defined _SKIA_SUPPORT_
  DebugVerifyDeviceIsPreMultiplied();
#endif
  bool bRet = false;
  switch (pObj->GetType()) {
    case CPDF_PageObject::TEXT:
      bRet = ProcessText(pObj->AsText(), pObj2Device, nullptr);
      break;
    case CPDF_PageObject::PATH:
      bRet = ProcessPath(pObj->AsPath(), pObj2Device);
      break;
    case CPDF_PageObject::IMAGE:
      bRet = ProcessImage(pObj->AsImage(), pObj2Device);
      break;
    case CPDF_PageObject::SHADING:
      ProcessShading(pObj->AsShading(), pObj2Device);
      return;
    case CPDF_PageObject::FORM:
      bRet = ProcessForm(pObj->AsForm(), pObj2Device);
      break;
  }
  if (!bRet)
    DrawObjWithBackground(pObj, pObj2Device);
#if defined _SKIA_SUPPORT_
  DebugVerifyDeviceIsPreMultiplied();
#endif
}

bool CPDF_RenderStatus::DrawObjWithBlend(CPDF_PageObject* pObj,
                                         const CFX_Matrix* pObj2Device) {
  bool bRet = false;
  switch (pObj->GetType()) {
    case CPDF_PageObject::PATH:
      bRet = ProcessPath(pObj->AsPath(), pObj2Device);
      break;
    case CPDF_PageObject::IMAGE:
      bRet = ProcessImage(pObj->AsImage(), pObj2Device);
      break;
    case CPDF_PageObject::FORM:
      bRet = ProcessForm(pObj->AsForm(), pObj2Device);
      break;
    default:
      break;
  }
  return bRet;
}
void CPDF_RenderStatus::GetScaledMatrix(CFX_Matrix& matrix) const {
  CFX_Matrix dCTM = m_pDevice->GetCTM();
  matrix.a *= FXSYS_fabs(dCTM.a);
  matrix.d *= FXSYS_fabs(dCTM.d);
}

void CPDF_RenderStatus::DrawObjWithBackground(CPDF_PageObject* pObj,
                                              const CFX_Matrix* pObj2Device) {
  FX_RECT rect;
  if (GetObjectClippedRect(pObj, pObj2Device, false, rect)) {
    return;
  }
  int res = 300;
  if (pObj->IsImage() &&
      m_pDevice->GetDeviceCaps(FXDC_DEVICE_CLASS) == FXDC_PRINTER) {
    res = 0;
  }
  CPDF_ScaledRenderBuffer buffer;
  if (!buffer.Initialize(m_pContext, m_pDevice, rect, pObj, &m_Options, res)) {
    return;
  }
  CFX_Matrix matrix = *pObj2Device;
  matrix.Concat(*buffer.GetMatrix());
  GetScaledMatrix(matrix);
  CPDF_Dictionary* pFormResource = nullptr;
  if (pObj->IsForm()) {
    const CPDF_FormObject* pFormObj = pObj->AsForm();
    if (pFormObj->m_pForm && pFormObj->m_pForm->m_pFormDict) {
      pFormResource = pFormObj->m_pForm->m_pFormDict->GetDictFor("Resources");
    }
  }
  CPDF_RenderStatus status;
  status.Initialize(m_pContext, buffer.GetDevice(), buffer.GetMatrix(), nullptr,
                    nullptr, nullptr, &m_Options, m_Transparency,
                    m_bDropObjects, pFormResource);
  status.RenderSingleObject(pObj, &matrix);
  buffer.OutputToDevice();
}

bool CPDF_RenderStatus::ProcessForm(const CPDF_FormObject* pFormObj,
                                    const CFX_Matrix* pObj2Device) {
#if defined _SKIA_SUPPORT_
  DebugVerifyDeviceIsPreMultiplied();
#endif
  CPDF_Dictionary* pOC = pFormObj->m_pForm->m_pFormDict->GetDictFor("OC");
  if (pOC && m_Options.m_pOCContext &&
      !m_Options.m_pOCContext->CheckOCGVisible(pOC)) {
    return true;
  }
  CFX_Matrix matrix = pFormObj->m_FormMatrix;
  matrix.Concat(*pObj2Device);
  CPDF_Dictionary* pResources = nullptr;
  if (pFormObj->m_pForm && pFormObj->m_pForm->m_pFormDict) {
    pResources = pFormObj->m_pForm->m_pFormDict->GetDictFor("Resources");
  }
  CPDF_RenderStatus status;
  status.Initialize(m_pContext, m_pDevice, nullptr, m_pStopObj, this, pFormObj,
                    &m_Options, m_Transparency, m_bDropObjects, pResources,
                    false);
  status.m_curBlend = m_curBlend;
  m_pDevice->SaveState();
  status.RenderObjectList(pFormObj->m_pForm.get(), &matrix);
  m_bStopped = status.m_bStopped;
  m_pDevice->RestoreState(false);
#if defined _SKIA_SUPPORT_
  DebugVerifyDeviceIsPreMultiplied();
#endif
  return true;
}

bool IsAvailableMatrix(const CFX_Matrix& matrix) {
  if (matrix.a == 0 || matrix.d == 0) {
    return matrix.b != 0 && matrix.c != 0;
  }
  if (matrix.b == 0 || matrix.c == 0) {
    return matrix.a != 0 && matrix.d != 0;
  }
  return true;
}

bool CPDF_RenderStatus::ProcessPath(CPDF_PathObject* pPathObj,
                                    const CFX_Matrix* pObj2Device) {
  int FillType = pPathObj->m_FillType;
  bool bStroke = pPathObj->m_bStroke;
  ProcessPathPattern(pPathObj, pObj2Device, FillType, bStroke);
  if (FillType == 0 && !bStroke)
    return true;

  uint32_t fill_argb = FillType ? GetFillArgb(pPathObj) : 0;
  uint32_t stroke_argb = bStroke ? GetStrokeArgb(pPathObj) : 0;
  CFX_Matrix path_matrix = pPathObj->m_Matrix;
  path_matrix.Concat(*pObj2Device);
  if (!IsAvailableMatrix(path_matrix))
    return true;

  if (FillType && (m_Options.m_Flags & RENDER_RECT_AA))
    FillType |= FXFILL_RECT_AA;
  if (m_Options.m_Flags & RENDER_FILL_FULLCOVER)
    FillType |= FXFILL_FULLCOVER;
  if (m_Options.m_Flags & RENDER_NOPATHSMOOTH)
    FillType |= FXFILL_NOPATHSMOOTH;
  if (bStroke)
    FillType |= FX_FILL_STROKE;

  const CPDF_PageObject* pPageObj =
      static_cast<const CPDF_PageObject*>(pPathObj);
  if (pPageObj->m_GeneralState.GetStrokeAdjust())
    FillType |= FX_STROKE_ADJUST;
  if (m_pType3Char)
    FillType |= FX_FILL_TEXT_MODE;

  CFX_GraphState graphState = pPathObj->m_GraphState;
  if (m_Options.m_Flags & RENDER_THINLINE)
    graphState.SetLineWidth(0);
  return m_pDevice->DrawPathWithBlend(
      pPathObj->m_Path.GetObject(), &path_matrix, graphState.GetObject(),
      fill_argb, stroke_argb, FillType, m_curBlend);
}

CPDF_TransferFunc* CPDF_RenderStatus::GetTransferFunc(CPDF_Object* pObj) const {
  ASSERT(pObj);
  CPDF_DocRenderData* pDocCache = m_pContext->GetDocument()->GetRenderData();
  return pDocCache ? pDocCache->GetTransferFunc(pObj) : nullptr;
}

FX_ARGB CPDF_RenderStatus::GetFillArgb(CPDF_PageObject* pObj,
                                       bool bType3) const {
  const CPDF_ColorState* pColorState = &pObj->m_ColorState;
  if (m_pType3Char && !bType3 &&
      (!m_pType3Char->m_bColored ||
       (m_pType3Char->m_bColored &&
        (!*pColorState || pColorState->GetFillColor()->IsNull())))) {
    return m_T3FillColor;
  }
  if (!*pColorState || pColorState->GetFillColor()->IsNull())
    pColorState = &m_InitialStates.m_ColorState;

  FX_COLORREF rgb = pColorState->GetFillRGB();
  if (rgb == (uint32_t)-1)
    return 0;

  int32_t alpha =
      static_cast<int32_t>((pObj->m_GeneralState.GetFillAlpha() * 255));
  if (pObj->m_GeneralState.GetTR()) {
    if (!pObj->m_GeneralState.GetTransferFunc()) {
      pObj->m_GeneralState.SetTransferFunc(
          GetTransferFunc(pObj->m_GeneralState.GetTR()));
    }
    if (pObj->m_GeneralState.GetTransferFunc())
      rgb = pObj->m_GeneralState.GetTransferFunc()->TranslateColor(rgb);
  }
  return m_Options.TranslateColor(ArgbEncode(alpha, rgb));
}

FX_ARGB CPDF_RenderStatus::GetStrokeArgb(CPDF_PageObject* pObj) const {
  const CPDF_ColorState* pColorState = &pObj->m_ColorState;
  if (m_pType3Char &&
      (!m_pType3Char->m_bColored ||
       (m_pType3Char->m_bColored &&
        (!*pColorState || pColorState->GetStrokeColor()->IsNull())))) {
    return m_T3FillColor;
  }
  if (!*pColorState || pColorState->GetStrokeColor()->IsNull())
    pColorState = &m_InitialStates.m_ColorState;

  FX_COLORREF rgb = pColorState->GetStrokeRGB();
  if (rgb == (uint32_t)-1)
    return 0;

  int32_t alpha = static_cast<int32_t>(pObj->m_GeneralState.GetStrokeAlpha() *
                                       255);  // not rounded.
  if (pObj->m_GeneralState.GetTR()) {
    if (!pObj->m_GeneralState.GetTransferFunc()) {
      pObj->m_GeneralState.SetTransferFunc(
          GetTransferFunc(pObj->m_GeneralState.GetTR()));
    }
    if (pObj->m_GeneralState.GetTransferFunc())
      rgb = pObj->m_GeneralState.GetTransferFunc()->TranslateColor(rgb);
  }
  return m_Options.TranslateColor(ArgbEncode(alpha, rgb));
}
void CPDF_RenderStatus::ProcessClipPath(CPDF_ClipPath ClipPath,
                                        const CFX_Matrix* pObj2Device) {
  if (!ClipPath) {
    if (m_LastClipPath) {
      m_pDevice->RestoreState(true);
      m_LastClipPath.SetNull();
    }
    return;
  }
  if (m_LastClipPath == ClipPath)
    return;

  m_LastClipPath = ClipPath;
  m_pDevice->RestoreState(true);
  int nClipPath = ClipPath.GetPathCount();
  for (int i = 0; i < nClipPath; ++i) {
    const CFX_PathData* pPathData = ClipPath.GetPath(i).GetObject();
    if (!pPathData)
      continue;

    if (pPathData->GetPointCount() == 0) {
      CFX_PathData EmptyPath;
      EmptyPath.AppendRect(-1, -1, 0, 0);
      int fill_mode = FXFILL_WINDING;
      m_pDevice->SetClip_PathFill(&EmptyPath, nullptr, fill_mode);
    } else {
      int ClipType = ClipPath.GetClipType(i);
      m_pDevice->SetClip_PathFill(pPathData, pObj2Device, ClipType);
    }
  }
  int textcount = ClipPath.GetTextCount();
  if (textcount == 0)
    return;

  if (m_pDevice->GetDeviceClass() == FXDC_DISPLAY &&
      !(m_pDevice->GetDeviceCaps(FXDC_RENDER_CAPS) & FXRC_SOFT_CLIP)) {
    return;
  }

  std::unique_ptr<CFX_PathData> pTextClippingPath;
  for (int i = 0; i < textcount; ++i) {
    CPDF_TextObject* pText = ClipPath.GetText(i);
    if (pText) {
      if (!pTextClippingPath)
        pTextClippingPath.reset(new CFX_PathData);
      ProcessText(pText, pObj2Device, pTextClippingPath.get());
      continue;
    }

    if (!pTextClippingPath)
      continue;

    int fill_mode = FXFILL_WINDING;
    if (m_Options.m_Flags & RENDER_NOTEXTSMOOTH)
      fill_mode |= FXFILL_NOPATHSMOOTH;
    m_pDevice->SetClip_PathFill(pTextClippingPath.get(), nullptr, fill_mode);
    pTextClippingPath.reset();
  }
}

void CPDF_RenderStatus::DrawClipPath(CPDF_ClipPath ClipPath,
                                     const CFX_Matrix* pObj2Device) {
  if (!ClipPath)
    return;

  int fill_mode = 0;
  if (m_Options.m_Flags & RENDER_NOPATHSMOOTH) {
    fill_mode |= FXFILL_NOPATHSMOOTH;
  }
  int nClipPath = ClipPath.GetPathCount();
  int i;
  for (i = 0; i < nClipPath; i++) {
    const CFX_PathData* pPathData = ClipPath.GetPath(i).GetObject();
    if (!pPathData) {
      continue;
    }
    CFX_GraphStateData stroke_state;
    if (m_Options.m_Flags & RENDER_THINLINE) {
      stroke_state.m_LineWidth = 0;
    }
    m_pDevice->DrawPath(pPathData, pObj2Device, &stroke_state, 0, 0xffff0000,
                        fill_mode);
  }
}
bool CPDF_RenderStatus::SelectClipPath(const CPDF_PathObject* pPathObj,
                                       const CFX_Matrix* pObj2Device,
                                       bool bStroke) {
  CFX_Matrix path_matrix = pPathObj->m_Matrix;
  path_matrix.Concat(*pObj2Device);
  if (bStroke) {
    CFX_GraphState graphState = pPathObj->m_GraphState;
    if (m_Options.m_Flags & RENDER_THINLINE)
      graphState.SetLineWidth(0);
    return m_pDevice->SetClip_PathStroke(pPathObj->m_Path.GetObject(),
                                         &path_matrix, graphState.GetObject());
  }
  int fill_mode = pPathObj->m_FillType;
  if (m_Options.m_Flags & RENDER_NOPATHSMOOTH) {
    fill_mode |= FXFILL_NOPATHSMOOTH;
  }
  return m_pDevice->SetClip_PathFill(pPathObj->m_Path.GetObject(), &path_matrix,
                                     fill_mode);
}
bool CPDF_RenderStatus::ProcessTransparency(CPDF_PageObject* pPageObj,
                                            const CFX_Matrix* pObj2Device) {
#if defined _SKIA_SUPPORT_
  DebugVerifyDeviceIsPreMultiplied();
#endif
  int blend_type = pPageObj->m_GeneralState.GetBlendType();
  if (blend_type == FXDIB_BLEND_UNSUPPORTED)
    return true;

  CPDF_Dictionary* pSMaskDict =
      ToDictionary(pPageObj->m_GeneralState.GetSoftMask());
  if (pSMaskDict) {
    if (pPageObj->IsImage() &&
        pPageObj->AsImage()->GetImage()->GetDict()->KeyExist("SMask")) {
      pSMaskDict = nullptr;
    }
  }
  CPDF_Dictionary* pFormResource = nullptr;
  FX_FLOAT group_alpha = 1.0f;
  int Transparency = m_Transparency;
  bool bGroupTransparent = false;
  if (pPageObj->IsForm()) {
    const CPDF_FormObject* pFormObj = pPageObj->AsForm();
    group_alpha = pFormObj->m_GeneralState.GetFillAlpha();
    Transparency = pFormObj->m_pForm->m_Transparency;
    bGroupTransparent = !!(Transparency & PDFTRANS_ISOLATED);
    if (pFormObj->m_pForm->m_pFormDict) {
      pFormResource = pFormObj->m_pForm->m_pFormDict->GetDictFor("Resources");
    }
  }
  bool bTextClip =
      (pPageObj->m_ClipPath && pPageObj->m_ClipPath.GetTextCount() &&
       m_pDevice->GetDeviceClass() == FXDC_DISPLAY &&
       !(m_pDevice->GetDeviceCaps(FXDC_RENDER_CAPS) & FXRC_SOFT_CLIP));
  if ((m_Options.m_Flags & RENDER_OVERPRINT) && pPageObj->IsImage() &&
      pPageObj->m_GeneralState.GetFillOP() &&
      pPageObj->m_GeneralState.GetStrokeOP()) {
    CPDF_Document* pDocument = nullptr;
    CPDF_Page* pPage = nullptr;
    if (m_pContext->GetPageCache()) {
      pPage = m_pContext->GetPageCache()->GetPage();
      pDocument = pPage->m_pDocument;
    } else {
      pDocument = pPageObj->AsImage()->GetImage()->GetDocument();
    }
    CPDF_Dictionary* pPageResources = pPage ? pPage->m_pPageResources : nullptr;
    CPDF_Object* pCSObj = pPageObj->AsImage()
                              ->GetImage()
                              ->GetStream()
                              ->GetDict()
                              ->GetDirectObjectFor("ColorSpace");
    CPDF_ColorSpace* pColorSpace =
        pDocument->LoadColorSpace(pCSObj, pPageResources);
    if (pColorSpace) {
      int format = pColorSpace->GetFamily();
      if (format == PDFCS_DEVICECMYK || format == PDFCS_SEPARATION ||
          format == PDFCS_DEVICEN) {
        blend_type = FXDIB_BLEND_DARKEN;
      }
      pDocument->GetPageData()->ReleaseColorSpace(pCSObj);
    }
  }
  if (!pSMaskDict && group_alpha == 1.0f && blend_type == FXDIB_BLEND_NORMAL &&
      !bTextClip && !bGroupTransparent) {
    return false;
  }
  bool isolated = !!(Transparency & PDFTRANS_ISOLATED);
  if (m_bPrint) {
    bool bRet = false;
    int rendCaps = m_pDevice->GetRenderCaps();
    if (!((Transparency & PDFTRANS_ISOLATED) || pSMaskDict || bTextClip) &&
        (rendCaps & FXRC_BLEND_MODE)) {
      int oldBlend = m_curBlend;
      m_curBlend = blend_type;
      bRet = DrawObjWithBlend(pPageObj, pObj2Device);
      m_curBlend = oldBlend;
    }
    if (!bRet) {
      DrawObjWithBackground(pPageObj, pObj2Device);
    }
    return true;
  }
  FX_RECT rect = pPageObj->GetBBox(pObj2Device);
  rect.Intersect(m_pDevice->GetClipBox());
  if (rect.IsEmpty()) {
    return true;
  }
  CFX_Matrix deviceCTM = m_pDevice->GetCTM();
  FX_FLOAT scaleX = FXSYS_fabs(deviceCTM.a);
  FX_FLOAT scaleY = FXSYS_fabs(deviceCTM.d);
  int width = FXSYS_round((FX_FLOAT)rect.Width() * scaleX);
  int height = FXSYS_round((FX_FLOAT)rect.Height() * scaleY);
  CFX_FxgeDevice bitmap_device;
  std::unique_ptr<CFX_DIBitmap> oriDevice;
  if (!isolated && (m_pDevice->GetRenderCaps() & FXRC_GET_BITS)) {
    oriDevice.reset(new CFX_DIBitmap);
    if (!m_pDevice->CreateCompatibleBitmap(oriDevice.get(), width, height))
      return true;
    m_pDevice->GetDIBits(oriDevice.get(), rect.left, rect.top);
  }
  if (!bitmap_device.Create(width, height, FXDIB_Argb, oriDevice.get()))
    return true;
  CFX_DIBitmap* bitmap = bitmap_device.GetBitmap();
  bitmap->Clear(0);
  CFX_Matrix new_matrix = *pObj2Device;
  new_matrix.TranslateI(-rect.left, -rect.top);
  new_matrix.Scale(scaleX, scaleY);
  std::unique_ptr<CFX_DIBitmap> pTextMask;
  if (bTextClip) {
    pTextMask.reset(new CFX_DIBitmap);
    if (!pTextMask->Create(width, height, FXDIB_8bppMask))
      return true;

    pTextMask->Clear(0);
    CFX_FxgeDevice text_device;
    text_device.Attach(pTextMask.get(), false, nullptr, false);
    for (uint32_t i = 0; i < pPageObj->m_ClipPath.GetTextCount(); i++) {
      CPDF_TextObject* textobj = pPageObj->m_ClipPath.GetText(i);
      if (!textobj) {
        break;
      }
      CFX_Matrix text_matrix;
      textobj->GetTextMatrix(&text_matrix);
      CPDF_TextRenderer::DrawTextPath(
          &text_device, textobj->m_nChars, textobj->m_pCharCodes,
          textobj->m_pCharPos, textobj->m_TextState.GetFont(),
          textobj->m_TextState.GetFontSize(), &text_matrix, &new_matrix,
          textobj->m_GraphState.GetObject(), (FX_ARGB)-1, 0, nullptr, 0);
    }
  }
  CPDF_RenderStatus bitmap_render;
  bitmap_render.Initialize(m_pContext, &bitmap_device, nullptr, m_pStopObj,
                           nullptr, nullptr, &m_Options, 0, m_bDropObjects,
                           pFormResource, true);
  bitmap_render.ProcessObjectNoClip(pPageObj, &new_matrix);
  m_bStopped = bitmap_render.m_bStopped;
  if (pSMaskDict) {
    CFX_Matrix smask_matrix = *pPageObj->m_GeneralState.GetSMaskMatrix();
    smask_matrix.Concat(*pObj2Device);
    std::unique_ptr<CFX_DIBSource> pSMaskSource(
        LoadSMask(pSMaskDict, &rect, &smask_matrix));
    if (pSMaskSource)
      bitmap->MultiplyAlpha(pSMaskSource.get());
  }
  if (pTextMask) {
    bitmap->MultiplyAlpha(pTextMask.get());
    pTextMask.reset();
  }
  int32_t blitAlpha = 255;
  if (Transparency & PDFTRANS_GROUP && group_alpha != 1.0f) {
    blitAlpha = (int32_t)(group_alpha * 255);
#ifndef _SKIA_SUPPORT_
    bitmap->MultiplyAlpha(blitAlpha);
    blitAlpha = 255;
#endif
  }
  Transparency = m_Transparency;
  if (pPageObj->IsForm()) {
    Transparency |= PDFTRANS_GROUP;
  }
  CompositeDIBitmap(bitmap, rect.left, rect.top, 0, blitAlpha, blend_type,
                    Transparency);
#if defined _SKIA_SUPPORT_
  DebugVerifyDeviceIsPreMultiplied();
#endif
  return true;
}

CFX_DIBitmap* CPDF_RenderStatus::GetBackdrop(const CPDF_PageObject* pObj,
                                             const FX_RECT& rect,
                                             int& left,
                                             int& top,
                                             bool bBackAlphaRequired) {
  FX_RECT bbox = rect;
  bbox.Intersect(m_pDevice->GetClipBox());
  left = bbox.left;
  top = bbox.top;
  CFX_Matrix deviceCTM = m_pDevice->GetCTM();
  FX_FLOAT scaleX = FXSYS_fabs(deviceCTM.a);
  FX_FLOAT scaleY = FXSYS_fabs(deviceCTM.d);
  int width = FXSYS_round(bbox.Width() * scaleX);
  int height = FXSYS_round(bbox.Height() * scaleY);
  std::unique_ptr<CFX_DIBitmap> pBackdrop(new CFX_DIBitmap);
  if (bBackAlphaRequired && !m_bDropObjects)
    pBackdrop->Create(width, height, FXDIB_Argb);
  else
    m_pDevice->CreateCompatibleBitmap(pBackdrop.get(), width, height);

  if (!pBackdrop->GetBuffer())
    return nullptr;

  bool bNeedDraw;
  if (pBackdrop->HasAlpha())
    bNeedDraw = !(m_pDevice->GetRenderCaps() & FXRC_ALPHA_OUTPUT);
  else
    bNeedDraw = !(m_pDevice->GetRenderCaps() & FXRC_GET_BITS);

  if (!bNeedDraw) {
    m_pDevice->GetDIBits(pBackdrop.get(), left, top);
    return pBackdrop.release();
  }

  CFX_Matrix FinalMatrix = m_DeviceMatrix;
  FinalMatrix.TranslateI(-left, -top);
  FinalMatrix.Scale(scaleX, scaleY);
  pBackdrop->Clear(pBackdrop->HasAlpha() ? 0 : 0xffffffff);
  CFX_FxgeDevice device;
  device.Attach(pBackdrop.get(), false, nullptr, false);
  m_pContext->Render(&device, pObj, &m_Options, &FinalMatrix);
  return pBackdrop.release();
}

void CPDF_RenderContext::GetBackground(CFX_DIBitmap* pBuffer,
                                       const CPDF_PageObject* pObj,
                                       const CPDF_RenderOptions* pOptions,
                                       CFX_Matrix* pFinalMatrix) {
  CFX_FxgeDevice device;
  device.Attach(pBuffer, false, nullptr, false);

  FX_RECT rect(0, 0, device.GetWidth(), device.GetHeight());
  device.FillRect(&rect, 0xffffffff);
  Render(&device, pObj, pOptions, pFinalMatrix);
}
CPDF_GraphicStates* CPDF_RenderStatus::CloneObjStates(
    const CPDF_GraphicStates* pSrcStates,
    bool bStroke) {
  if (!pSrcStates)
    return nullptr;

  CPDF_GraphicStates* pStates = new CPDF_GraphicStates;
  pStates->CopyStates(*pSrcStates);
  const CPDF_Color* pObjColor = bStroke
                                    ? pSrcStates->m_ColorState.GetStrokeColor()
                                    : pSrcStates->m_ColorState.GetFillColor();
  if (!pObjColor->IsNull()) {
    pStates->m_ColorState.SetFillRGB(
        bStroke ? pSrcStates->m_ColorState.GetStrokeRGB()
                : pSrcStates->m_ColorState.GetFillRGB());
    pStates->m_ColorState.SetStrokeRGB(pStates->m_ColorState.GetFillRGB());
  }
  return pStates;
}

CPDF_RenderContext::CPDF_RenderContext(CPDF_Page* pPage)
    : m_pDocument(pPage->m_pDocument),
      m_pPageResources(pPage->m_pPageResources),
      m_pPageCache(pPage->GetRenderCache()) {}

CPDF_RenderContext::CPDF_RenderContext(CPDF_Document* pDoc,
                                       CPDF_PageRenderCache* pPageCache)
    : m_pDocument(pDoc), m_pPageResources(nullptr), m_pPageCache(pPageCache) {}

CPDF_RenderContext::~CPDF_RenderContext() {}

void CPDF_RenderContext::AppendLayer(CPDF_PageObjectHolder* pObjectHolder,
                                     const CFX_Matrix* pObject2Device) {
  Layer* pLayer = m_Layers.AddSpace();
  pLayer->m_pObjectHolder = pObjectHolder;
  if (pObject2Device) {
    pLayer->m_Matrix = *pObject2Device;
  } else {
    pLayer->m_Matrix.SetIdentity();
  }
}
void CPDF_RenderContext::Render(CFX_RenderDevice* pDevice,
                                const CPDF_RenderOptions* pOptions,
                                const CFX_Matrix* pLastMatrix) {
  Render(pDevice, nullptr, pOptions, pLastMatrix);
}
void CPDF_RenderContext::Render(CFX_RenderDevice* pDevice,
                                const CPDF_PageObject* pStopObj,
                                const CPDF_RenderOptions* pOptions,
                                const CFX_Matrix* pLastMatrix) {
  int count = m_Layers.GetSize();
  for (int j = 0; j < count; j++) {
    pDevice->SaveState();
    Layer* pLayer = m_Layers.GetDataPtr(j);
    if (pLastMatrix) {
      CFX_Matrix FinalMatrix = pLayer->m_Matrix;
      FinalMatrix.Concat(*pLastMatrix);
      CPDF_RenderStatus status;
      status.Initialize(this, pDevice, pLastMatrix, pStopObj, nullptr, nullptr,
                        pOptions, pLayer->m_pObjectHolder->m_Transparency,
                        false, nullptr);
      status.RenderObjectList(pLayer->m_pObjectHolder, &FinalMatrix);
      if (status.m_Options.m_Flags & RENDER_LIMITEDIMAGECACHE) {
        m_pPageCache->CacheOptimization(status.m_Options.m_dwLimitCacheSize);
      }
      if (status.m_bStopped) {
        pDevice->RestoreState(false);
        break;
      }
    } else {
      CPDF_RenderStatus status;
      status.Initialize(this, pDevice, nullptr, pStopObj, nullptr, nullptr,
                        pOptions, pLayer->m_pObjectHolder->m_Transparency,
                        false, nullptr);
      status.RenderObjectList(pLayer->m_pObjectHolder, &pLayer->m_Matrix);
      if (status.m_Options.m_Flags & RENDER_LIMITEDIMAGECACHE) {
        m_pPageCache->CacheOptimization(status.m_Options.m_dwLimitCacheSize);
      }
      if (status.m_bStopped) {
        pDevice->RestoreState(false);
        break;
      }
    }
    pDevice->RestoreState(false);
  }
}

CPDF_ProgressiveRenderer::CPDF_ProgressiveRenderer(
    CPDF_RenderContext* pContext,
    CFX_RenderDevice* pDevice,
    const CPDF_RenderOptions* pOptions)
    : m_Status(Ready),
      m_pContext(pContext),
      m_pDevice(pDevice),
      m_pOptions(pOptions),
      m_LayerIndex(0),
      m_pCurrentLayer(nullptr) {}

CPDF_ProgressiveRenderer::~CPDF_ProgressiveRenderer() {
  if (m_pRenderStatus)
    m_pDevice->RestoreState(false);
}

void CPDF_ProgressiveRenderer::Start(IFX_Pause* pPause) {
  if (!m_pContext || !m_pDevice || m_Status != Ready) {
    m_Status = Failed;
    return;
  }
  m_Status = ToBeContinued;
  Continue(pPause);
}

void CPDF_ProgressiveRenderer::Continue(IFX_Pause* pPause) {
  while (m_Status == ToBeContinued) {
    if (!m_pCurrentLayer) {
      if (m_LayerIndex >= m_pContext->CountLayers()) {
        m_Status = Done;
        return;
      }
      m_pCurrentLayer = m_pContext->GetLayer(m_LayerIndex);
      m_LastObjectRendered =
          m_pCurrentLayer->m_pObjectHolder->GetPageObjectList()->end();
      m_pRenderStatus.reset(new CPDF_RenderStatus());
      m_pRenderStatus->Initialize(
          m_pContext, m_pDevice, nullptr, nullptr, nullptr, nullptr, m_pOptions,
          m_pCurrentLayer->m_pObjectHolder->m_Transparency, false, nullptr);
      m_pDevice->SaveState();
      m_ClipRect = CFX_FloatRect(m_pDevice->GetClipBox());
      CFX_Matrix device2object;
      device2object.SetReverse(m_pCurrentLayer->m_Matrix);
      device2object.TransformRect(m_ClipRect);
    }
    CPDF_PageObjectList::iterator iter;
    CPDF_PageObjectList::iterator iterEnd =
        m_pCurrentLayer->m_pObjectHolder->GetPageObjectList()->end();
    if (m_LastObjectRendered != iterEnd) {
      iter = m_LastObjectRendered;
      ++iter;
    } else {
      iter = m_pCurrentLayer->m_pObjectHolder->GetPageObjectList()->begin();
    }
    int nObjsToGo = kStepLimit;
    while (iter != iterEnd) {
      CPDF_PageObject* pCurObj = iter->get();
      if (pCurObj && pCurObj->m_Left <= m_ClipRect.right &&
          pCurObj->m_Right >= m_ClipRect.left &&
          pCurObj->m_Bottom <= m_ClipRect.top &&
          pCurObj->m_Top >= m_ClipRect.bottom) {
        if (m_pRenderStatus->ContinueSingleObject(
                pCurObj, &m_pCurrentLayer->m_Matrix, pPause)) {
          return;
        }
        if (pCurObj->IsImage() &&
            m_pRenderStatus->m_Options.m_Flags & RENDER_LIMITEDIMAGECACHE) {
          m_pContext->GetPageCache()->CacheOptimization(
              m_pRenderStatus->m_Options.m_dwLimitCacheSize);
        }
        if (pCurObj->IsForm() || pCurObj->IsShading()) {
          nObjsToGo = 0;
        } else {
          --nObjsToGo;
        }
      }
      m_LastObjectRendered = iter;
      if (nObjsToGo == 0) {
        if (pPause && pPause->NeedToPauseNow())
          return;
        nObjsToGo = kStepLimit;
      }
      ++iter;
    }
    if (m_pCurrentLayer->m_pObjectHolder->IsParsed()) {
      m_pRenderStatus.reset();
      m_pDevice->RestoreState(false);
      m_pCurrentLayer = nullptr;
      m_LayerIndex++;
      if (pPause && pPause->NeedToPauseNow()) {
        return;
      }
    } else {
      m_pCurrentLayer->m_pObjectHolder->ContinueParse(pPause);
      if (!m_pCurrentLayer->m_pObjectHolder->IsParsed())
        return;
    }
  }
}

CPDF_DeviceBuffer::CPDF_DeviceBuffer()
    : m_pDevice(nullptr), m_pContext(nullptr), m_pObject(nullptr) {}

CPDF_DeviceBuffer::~CPDF_DeviceBuffer() {}

bool CPDF_DeviceBuffer::Initialize(CPDF_RenderContext* pContext,
                                   CFX_RenderDevice* pDevice,
                                   FX_RECT* pRect,
                                   const CPDF_PageObject* pObj,
                                   int max_dpi) {
  m_pDevice = pDevice;
  m_pContext = pContext;
  m_Rect = *pRect;
  m_pObject = pObj;
  m_Matrix.TranslateI(-pRect->left, -pRect->top);
#if _FXM_PLATFORM_ != _FXM_PLATFORM_APPLE_
  int horz_size = pDevice->GetDeviceCaps(FXDC_HORZ_SIZE);
  int vert_size = pDevice->GetDeviceCaps(FXDC_VERT_SIZE);
  if (horz_size && vert_size && max_dpi) {
    int dpih =
        pDevice->GetDeviceCaps(FXDC_PIXEL_WIDTH) * 254 / (horz_size * 10);
    int dpiv =
        pDevice->GetDeviceCaps(FXDC_PIXEL_HEIGHT) * 254 / (vert_size * 10);
    if (dpih > max_dpi) {
      m_Matrix.Scale((FX_FLOAT)(max_dpi) / dpih, 1.0f);
    }
    if (dpiv > max_dpi) {
      m_Matrix.Scale(1.0f, (FX_FLOAT)(max_dpi) / (FX_FLOAT)dpiv);
    }
  }
#endif
  CFX_Matrix ctm = m_pDevice->GetCTM();
  FX_FLOAT fScaleX = FXSYS_fabs(ctm.a);
  FX_FLOAT fScaleY = FXSYS_fabs(ctm.d);
  m_Matrix.Concat(fScaleX, 0, 0, fScaleY, 0, 0);
  CFX_FloatRect rect(*pRect);
  m_Matrix.TransformRect(rect);
  FX_RECT bitmap_rect = rect.GetOuterRect();
  m_pBitmap.reset(new CFX_DIBitmap);
  m_pBitmap->Create(bitmap_rect.Width(), bitmap_rect.Height(), FXDIB_Argb);
  return true;
}
void CPDF_DeviceBuffer::OutputToDevice() {
  if (m_pDevice->GetDeviceCaps(FXDC_RENDER_CAPS) & FXRC_GET_BITS) {
    if (m_Matrix.a == 1.0f && m_Matrix.d == 1.0f) {
      m_pDevice->SetDIBits(m_pBitmap.get(), m_Rect.left, m_Rect.top);
    } else {
      m_pDevice->StretchDIBits(m_pBitmap.get(), m_Rect.left, m_Rect.top,
                               m_Rect.Width(), m_Rect.Height());
    }
  } else {
    CFX_DIBitmap buffer;
    m_pDevice->CreateCompatibleBitmap(&buffer, m_pBitmap->GetWidth(),
                                      m_pBitmap->GetHeight());
    m_pContext->GetBackground(&buffer, m_pObject, nullptr, &m_Matrix);
    buffer.CompositeBitmap(0, 0, buffer.GetWidth(), buffer.GetHeight(),
                           m_pBitmap.get(), 0, 0);
    m_pDevice->StretchDIBits(&buffer, m_Rect.left, m_Rect.top, m_Rect.Width(),
                             m_Rect.Height());
  }
}

CPDF_ScaledRenderBuffer::CPDF_ScaledRenderBuffer() {}

CPDF_ScaledRenderBuffer::~CPDF_ScaledRenderBuffer() {}

#define _FPDFAPI_IMAGESIZE_LIMIT_ (30 * 1024 * 1024)
bool CPDF_ScaledRenderBuffer::Initialize(CPDF_RenderContext* pContext,
                                         CFX_RenderDevice* pDevice,
                                         const FX_RECT& pRect,
                                         const CPDF_PageObject* pObj,
                                         const CPDF_RenderOptions* pOptions,
                                         int max_dpi) {
  m_pDevice = pDevice;
  if (m_pDevice->GetDeviceCaps(FXDC_RENDER_CAPS) & FXRC_GET_BITS) {
    return true;
  }
  m_pContext = pContext;
  m_Rect = pRect;
  m_pObject = pObj;
  m_Matrix.TranslateI(-pRect.left, -pRect.top);
  int horz_size = pDevice->GetDeviceCaps(FXDC_HORZ_SIZE);
  int vert_size = pDevice->GetDeviceCaps(FXDC_VERT_SIZE);
  if (horz_size && vert_size && max_dpi) {
    int dpih =
        pDevice->GetDeviceCaps(FXDC_PIXEL_WIDTH) * 254 / (horz_size * 10);
    int dpiv =
        pDevice->GetDeviceCaps(FXDC_PIXEL_HEIGHT) * 254 / (vert_size * 10);
    if (dpih > max_dpi) {
      m_Matrix.Scale((FX_FLOAT)(max_dpi) / dpih, 1.0f);
    }
    if (dpiv > max_dpi) {
      m_Matrix.Scale(1.0f, (FX_FLOAT)(max_dpi) / (FX_FLOAT)dpiv);
    }
  }
  m_pBitmapDevice.reset(new CFX_FxgeDevice);
  FXDIB_Format dibFormat = FXDIB_Rgb;
  int32_t bpp = 24;
  if (m_pDevice->GetDeviceCaps(FXDC_RENDER_CAPS) & FXRC_ALPHA_OUTPUT) {
    dibFormat = FXDIB_Argb;
    bpp = 32;
  }
  while (1) {
    CFX_FloatRect rect(pRect);
    m_Matrix.TransformRect(rect);
    FX_RECT bitmap_rect = rect.GetOuterRect();
    int32_t iWidth = bitmap_rect.Width();
    int32_t iHeight = bitmap_rect.Height();
    int32_t iPitch = (iWidth * bpp + 31) / 32 * 4;
    if (iWidth * iHeight < 1)
      return false;

    if (iPitch * iHeight <= _FPDFAPI_IMAGESIZE_LIMIT_ &&
        m_pBitmapDevice->Create(iWidth, iHeight, dibFormat, nullptr)) {
      break;
    }
    m_Matrix.Scale(0.5f, 0.5f);
  }
  m_pContext->GetBackground(m_pBitmapDevice->GetBitmap(), m_pObject, pOptions,
                            &m_Matrix);
  return true;
}
void CPDF_ScaledRenderBuffer::OutputToDevice() {
  if (m_pBitmapDevice) {
    m_pDevice->StretchDIBits(m_pBitmapDevice->GetBitmap(), m_Rect.left,
                             m_Rect.top, m_Rect.Width(), m_Rect.Height());
  }
}

#if defined _SKIA_SUPPORT_
void CPDF_RenderStatus::DebugVerifyDeviceIsPreMultiplied() const {
  m_pDevice->DebugVerifyBitmapIsPreMultiplied();
}
#endif
