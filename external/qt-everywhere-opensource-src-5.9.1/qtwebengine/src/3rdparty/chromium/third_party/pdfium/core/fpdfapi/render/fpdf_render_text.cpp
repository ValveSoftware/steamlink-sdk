// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/render/render_int.h"

#include <vector>

#include "core/fpdfapi/font/cpdf_cidfont.h"
#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fpdfapi/font/cpdf_type3char.h"
#include "core/fpdfapi/font/cpdf_type3font.h"
#include "core/fpdfapi/page/cpdf_docpagedata.h"
#include "core/fpdfapi/page/cpdf_form.h"
#include "core/fpdfapi/page/cpdf_imageobject.h"
#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fpdfapi/page/cpdf_pathobject.h"
#include "core/fpdfapi/page/cpdf_textobject.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/render/cpdf_docrenderdata.h"
#include "core/fpdfapi/render/cpdf_renderoptions.h"
#include "core/fpdfapi/render/cpdf_textrenderer.h"
#include "core/fpdfapi/render/cpdf_type3cache.h"
#include "core/fxge/cfx_facecache.h"
#include "core/fxge/cfx_fxgedevice.h"
#include "core/fxge/cfx_gemodule.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"
#include "third_party/base/numerics/safe_math.h"

bool CPDF_RenderStatus::ProcessText(CPDF_TextObject* textobj,
                                    const CFX_Matrix* pObj2Device,
                                    CFX_PathData* pClippingPath) {
  if (textobj->m_nChars == 0)
    return true;

  const TextRenderingMode text_render_mode = textobj->m_TextState.GetTextMode();
  if (text_render_mode == TextRenderingMode::MODE_INVISIBLE)
    return true;

  CPDF_Font* pFont = textobj->m_TextState.GetFont();
  if (pFont->IsType3Font())
    return ProcessType3Text(textobj, pObj2Device);

  bool bFill = false;
  bool bStroke = false;
  bool bClip = false;
  if (pClippingPath) {
    bClip = true;
  } else {
    switch (text_render_mode) {
      case TextRenderingMode::MODE_FILL:
      case TextRenderingMode::MODE_FILL_CLIP:
        bFill = true;
        break;
      case TextRenderingMode::MODE_STROKE:
      case TextRenderingMode::MODE_STROKE_CLIP:
        if (pFont->GetFace())
          bStroke = true;
        else
          bFill = true;
        break;
      case TextRenderingMode::MODE_FILL_STROKE:
      case TextRenderingMode::MODE_FILL_STROKE_CLIP:
        bFill = true;
        if (pFont->GetFace())
          bStroke = true;
        break;
      case TextRenderingMode::MODE_INVISIBLE:
        // Already handled above, but the compiler is not smart enough to
        // realize it. Fall through.
        ASSERT(false);
      case TextRenderingMode::MODE_CLIP:
        return true;
    }
  }
  FX_ARGB stroke_argb = 0;
  FX_ARGB fill_argb = 0;
  bool bPattern = false;
  if (bStroke) {
    if (textobj->m_ColorState.GetStrokeColor()->IsPattern()) {
      bPattern = true;
    } else {
      stroke_argb = GetStrokeArgb(textobj);
    }
  }
  if (bFill) {
    if (textobj->m_ColorState.GetFillColor()->IsPattern()) {
      bPattern = true;
    } else {
      fill_argb = GetFillArgb(textobj);
    }
  }
  CFX_Matrix text_matrix;
  textobj->GetTextMatrix(&text_matrix);
  if (!IsAvailableMatrix(text_matrix))
    return true;

  FX_FLOAT font_size = textobj->m_TextState.GetFontSize();
  if (bPattern) {
    DrawTextPathWithPattern(textobj, pObj2Device, pFont, font_size,
                            &text_matrix, bFill, bStroke);
    return true;
  }
  if (bClip || bStroke) {
    const CFX_Matrix* pDeviceMatrix = pObj2Device;
    CFX_Matrix device_matrix;
    if (bStroke) {
      const FX_FLOAT* pCTM = textobj->m_TextState.GetCTM();
      if (pCTM[0] != 1.0f || pCTM[3] != 1.0f) {
        CFX_Matrix ctm(pCTM[0], pCTM[1], pCTM[2], pCTM[3], 0, 0);
        text_matrix.ConcatInverse(ctm);
        device_matrix = ctm;
        device_matrix.Concat(*pObj2Device);
        pDeviceMatrix = &device_matrix;
      }
    }
    int flag = 0;
    if (bStroke && bFill) {
      flag |= FX_FILL_STROKE;
      flag |= FX_STROKE_TEXT_MODE;
    }
    if (textobj->m_GeneralState.GetStrokeAdjust())
      flag |= FX_STROKE_ADJUST;
    if (m_Options.m_Flags & RENDER_NOTEXTSMOOTH)
      flag |= FXFILL_NOPATHSMOOTH;
    return CPDF_TextRenderer::DrawTextPath(
        m_pDevice, textobj->m_nChars, textobj->m_pCharCodes,
        textobj->m_pCharPos, pFont, font_size, &text_matrix, pDeviceMatrix,
        textobj->m_GraphState.GetObject(), fill_argb, stroke_argb,
        pClippingPath, flag);
  }
  text_matrix.Concat(*pObj2Device);
  return CPDF_TextRenderer::DrawNormalText(
      m_pDevice, textobj->m_nChars, textobj->m_pCharCodes, textobj->m_pCharPos,
      pFont, font_size, &text_matrix, fill_argb, &m_Options);
}

CPDF_Type3Cache* CPDF_RenderStatus::GetCachedType3(CPDF_Type3Font* pFont) {
  if (!pFont->m_pDocument) {
    return nullptr;
  }
  pFont->m_pDocument->GetPageData()->GetFont(pFont->GetFontDict());
  return pFont->m_pDocument->GetRenderData()->GetCachedType3(pFont);
}
static void ReleaseCachedType3(CPDF_Type3Font* pFont) {
  if (!pFont->m_pDocument) {
    return;
  }
  pFont->m_pDocument->GetRenderData()->ReleaseCachedType3(pFont);
  pFont->m_pDocument->GetPageData()->ReleaseFont(pFont->GetFontDict());
}

class CPDF_RefType3Cache {
 public:
  explicit CPDF_RefType3Cache(CPDF_Type3Font* pType3Font)
      : m_dwCount(0), m_pType3Font(pType3Font) {}
  ~CPDF_RefType3Cache() {
    while (m_dwCount--) {
      ReleaseCachedType3(m_pType3Font);
    }
  }
  uint32_t m_dwCount;
  CPDF_Type3Font* const m_pType3Font;
};

// TODO(npm): Font fallback for type 3 fonts? (Completely separate code!!)
bool CPDF_RenderStatus::ProcessType3Text(CPDF_TextObject* textobj,
                                         const CFX_Matrix* pObj2Device) {
  CPDF_Type3Font* pType3Font = textobj->m_TextState.GetFont()->AsType3Font();
  for (int i = 0; i < m_Type3FontCache.GetSize(); ++i) {
    if (m_Type3FontCache.GetAt(i) == pType3Font)
      return true;
  }

  CFX_Matrix dCTM = m_pDevice->GetCTM();
  FX_FLOAT sa = FXSYS_fabs(dCTM.a);
  FX_FLOAT sd = FXSYS_fabs(dCTM.d);
  CFX_Matrix text_matrix;
  textobj->GetTextMatrix(&text_matrix);
  CFX_Matrix char_matrix = pType3Font->GetFontMatrix();
  FX_FLOAT font_size = textobj->m_TextState.GetFontSize();
  char_matrix.Scale(font_size, font_size);
  FX_ARGB fill_argb = GetFillArgb(textobj, true);
  int fill_alpha = FXARGB_A(fill_argb);
  int device_class = m_pDevice->GetDeviceClass();
  std::vector<FXTEXT_GLYPHPOS> glyphs;
  if (device_class == FXDC_DISPLAY)
    glyphs.resize(textobj->m_nChars);
  else if (fill_alpha < 255)
    return false;

  CPDF_RefType3Cache refTypeCache(pType3Font);
  uint32_t* pChars = textobj->m_pCharCodes;
  if (textobj->m_nChars == 1)
    pChars = (uint32_t*)(&textobj->m_pCharCodes);

  for (int iChar = 0; iChar < textobj->m_nChars; iChar++) {
    uint32_t charcode = pChars[iChar];
    if (charcode == (uint32_t)-1)
      continue;

    CPDF_Type3Char* pType3Char = pType3Font->LoadChar(charcode);
    if (!pType3Char)
      continue;

    CFX_Matrix matrix = char_matrix;
    matrix.e += iChar ? textobj->m_pCharPos[iChar - 1] : 0;
    matrix.Concat(text_matrix);
    matrix.Concat(*pObj2Device);
    if (!pType3Char->LoadBitmap(m_pContext)) {
      if (!glyphs.empty()) {
        for (int i = 0; i < iChar; i++) {
          const FXTEXT_GLYPHPOS& glyph = glyphs[i];
          if (!glyph.m_pGlyph)
            continue;

          m_pDevice->SetBitMask(&glyph.m_pGlyph->m_Bitmap,
                                glyph.m_OriginX + glyph.m_pGlyph->m_Left,
                                glyph.m_OriginY - glyph.m_pGlyph->m_Top,
                                fill_argb);
        }
        glyphs.clear();
      }
      CPDF_GraphicStates* pStates = CloneObjStates(textobj, false);
      CPDF_RenderOptions Options = m_Options;
      Options.m_Flags |= RENDER_FORCE_HALFTONE | RENDER_RECT_AA;
      Options.m_Flags &= ~RENDER_FORCE_DOWNSAMPLE;
      CPDF_Dictionary* pFormResource = nullptr;
      if (pType3Char->m_pForm && pType3Char->m_pForm->m_pFormDict) {
        pFormResource =
            pType3Char->m_pForm->m_pFormDict->GetDictFor("Resources");
      }
      if (fill_alpha == 255) {
        CPDF_RenderStatus status;
        status.Initialize(m_pContext, m_pDevice, nullptr, nullptr, this,
                          pStates, &Options,
                          pType3Char->m_pForm->m_Transparency, m_bDropObjects,
                          pFormResource, false, pType3Char, fill_argb);
        status.m_Type3FontCache.Append(m_Type3FontCache);
        status.m_Type3FontCache.Add(pType3Font);
        m_pDevice->SaveState();
        status.RenderObjectList(pType3Char->m_pForm.get(), &matrix);
        m_pDevice->RestoreState(false);
      } else {
        CFX_FloatRect rect_f = pType3Char->m_pForm->CalcBoundingBox();
        rect_f.Transform(&matrix);
        FX_RECT rect = rect_f.GetOuterRect();
        CFX_FxgeDevice bitmap_device;
        if (!bitmap_device.Create((int)(rect.Width() * sa),
                                  (int)(rect.Height() * sd), FXDIB_Argb,
                                  nullptr)) {
          return true;
        }
        bitmap_device.GetBitmap()->Clear(0);
        CPDF_RenderStatus status;
        status.Initialize(m_pContext, &bitmap_device, nullptr, nullptr, this,
                          pStates, &Options,
                          pType3Char->m_pForm->m_Transparency, m_bDropObjects,
                          pFormResource, false, pType3Char, fill_argb);
        status.m_Type3FontCache.Append(m_Type3FontCache);
        status.m_Type3FontCache.Add(pType3Font);
        matrix.TranslateI(-rect.left, -rect.top);
        matrix.Scale(sa, sd);
        status.RenderObjectList(pType3Char->m_pForm.get(), &matrix);
        m_pDevice->SetDIBits(bitmap_device.GetBitmap(), rect.left, rect.top);
      }
      delete pStates;
    } else if (pType3Char->m_pBitmap) {
      if (device_class == FXDC_DISPLAY) {
        CPDF_Type3Cache* pCache = GetCachedType3(pType3Font);
        refTypeCache.m_dwCount++;
        CFX_GlyphBitmap* pBitmap = pCache->LoadGlyph(charcode, &matrix, sa, sd);
        if (!pBitmap)
          continue;

        int origin_x = FXSYS_round(matrix.e);
        int origin_y = FXSYS_round(matrix.f);
        if (glyphs.empty()) {
          m_pDevice->SetBitMask(&pBitmap->m_Bitmap, origin_x + pBitmap->m_Left,
                                origin_y - pBitmap->m_Top, fill_argb);
        } else {
          glyphs[iChar].m_pGlyph = pBitmap;
          glyphs[iChar].m_OriginX = origin_x;
          glyphs[iChar].m_OriginY = origin_y;
        }
      } else {
        CFX_Matrix image_matrix = pType3Char->m_ImageMatrix;
        image_matrix.Concat(matrix);
        CPDF_ImageRenderer renderer;
        if (renderer.Start(this, pType3Char->m_pBitmap.get(), fill_argb, 255,
                           &image_matrix, 0, false)) {
          renderer.Continue(nullptr);
        }
        if (!renderer.m_Result)
          return false;
      }
    }
  }

  if (glyphs.empty())
    return true;

  FX_RECT rect = FXGE_GetGlyphsBBox(glyphs, 0, sa, sd);
  CFX_DIBitmap bitmap;
  if (!bitmap.Create(static_cast<int>(rect.Width() * sa),
                     static_cast<int>(rect.Height() * sd), FXDIB_8bppMask)) {
    return true;
  }
  bitmap.Clear(0);
  for (const FXTEXT_GLYPHPOS& glyph : glyphs) {
    if (!glyph.m_pGlyph)
      continue;

    pdfium::base::CheckedNumeric<int> left = glyph.m_OriginX;
    left += glyph.m_pGlyph->m_Left;
    left -= rect.left;
    left *= sa;
    if (!left.IsValid())
      continue;

    pdfium::base::CheckedNumeric<int> top = glyph.m_OriginY;
    top -= glyph.m_pGlyph->m_Top;
    top -= rect.top;
    top *= sd;
    if (!top.IsValid())
      continue;

    bitmap.CompositeMask(left.ValueOrDie(), top.ValueOrDie(),
                         glyph.m_pGlyph->m_Bitmap.GetWidth(),
                         glyph.m_pGlyph->m_Bitmap.GetHeight(),
                         &glyph.m_pGlyph->m_Bitmap, fill_argb, 0, 0,
                         FXDIB_BLEND_NORMAL, nullptr, false, 0, nullptr);
  }
  m_pDevice->SetBitMask(&bitmap, rect.left, rect.top, fill_argb);
  return true;
}

class CPDF_CharPosList {
 public:
  CPDF_CharPosList();
  ~CPDF_CharPosList();
  void Load(int nChars,
            uint32_t* pCharCodes,
            FX_FLOAT* pCharPos,
            CPDF_Font* pFont,
            FX_FLOAT font_size);
  FXTEXT_CHARPOS* m_pCharPos;
  uint32_t m_nChars;
};

CPDF_CharPosList::CPDF_CharPosList() {
  m_pCharPos = nullptr;
}

CPDF_CharPosList::~CPDF_CharPosList() {
  FX_Free(m_pCharPos);
}

void CPDF_CharPosList::Load(int nChars,
                            uint32_t* pCharCodes,
                            FX_FLOAT* pCharPos,
                            CPDF_Font* pFont,
                            FX_FLOAT FontSize) {
  m_pCharPos = FX_Alloc(FXTEXT_CHARPOS, nChars);
  m_nChars = 0;
  CPDF_CIDFont* pCIDFont = pFont->AsCIDFont();
  bool bVertWriting = pCIDFont && pCIDFont->IsVertWriting();
  for (int iChar = 0; iChar < nChars; iChar++) {
    uint32_t CharCode =
        nChars == 1 ? (uint32_t)(uintptr_t)pCharCodes : pCharCodes[iChar];
    if (CharCode == (uint32_t)-1) {
      continue;
    }
    bool bVert = false;
    FXTEXT_CHARPOS& charpos = m_pCharPos[m_nChars++];
    if (pCIDFont) {
      charpos.m_bFontStyle = true;
    }
    charpos.m_GlyphIndex = pFont->GlyphFromCharCode(CharCode, &bVert);
    if (charpos.m_GlyphIndex != static_cast<uint32_t>(-1)) {
      charpos.m_FallbackFontPosition = -1;
    } else {
      charpos.m_FallbackFontPosition =
          pFont->FallbackFontFromCharcode(CharCode);
      charpos.m_GlyphIndex = pFont->FallbackGlyphFromCharcode(
          charpos.m_FallbackFontPosition, CharCode);
    }
// TODO(npm): Figure out how this affects m_ExtGID
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
    charpos.m_ExtGID = pFont->GlyphFromCharCodeExt(CharCode);
#endif
    if (!pFont->IsEmbedded() && !pFont->IsCIDFont()) {
      charpos.m_FontCharWidth = pFont->GetCharWidthF(CharCode);
    } else {
      charpos.m_FontCharWidth = 0;
    }
    charpos.m_OriginX = iChar ? pCharPos[iChar - 1] : 0;
    charpos.m_OriginY = 0;
    charpos.m_bGlyphAdjust = false;
    if (!pCIDFont) {
      continue;
    }
    uint16_t CID = pCIDFont->CIDFromCharCode(CharCode);
    if (bVertWriting) {
      charpos.m_OriginY = charpos.m_OriginX;
      charpos.m_OriginX = 0;
      short vx, vy;
      pCIDFont->GetVertOrigin(CID, vx, vy);
      charpos.m_OriginX -= FontSize * vx / 1000;
      charpos.m_OriginY -= FontSize * vy / 1000;
    }
    const uint8_t* pTransform = pCIDFont->GetCIDTransform(CID);
    if (pTransform && !bVert) {
      charpos.m_AdjustMatrix[0] = pCIDFont->CIDTransformToFloat(pTransform[0]);
      charpos.m_AdjustMatrix[2] = pCIDFont->CIDTransformToFloat(pTransform[2]);
      charpos.m_AdjustMatrix[1] = pCIDFont->CIDTransformToFloat(pTransform[1]);
      charpos.m_AdjustMatrix[3] = pCIDFont->CIDTransformToFloat(pTransform[3]);
      charpos.m_OriginX +=
          pCIDFont->CIDTransformToFloat(pTransform[4]) * FontSize;
      charpos.m_OriginY +=
          pCIDFont->CIDTransformToFloat(pTransform[5]) * FontSize;
      charpos.m_bGlyphAdjust = true;
    }
  }
}

// static
bool CPDF_TextRenderer::DrawTextPath(CFX_RenderDevice* pDevice,
                                     int nChars,
                                     uint32_t* pCharCodes,
                                     FX_FLOAT* pCharPos,
                                     CPDF_Font* pFont,
                                     FX_FLOAT font_size,
                                     const CFX_Matrix* pText2User,
                                     const CFX_Matrix* pUser2Device,
                                     const CFX_GraphStateData* pGraphState,
                                     FX_ARGB fill_argb,
                                     FX_ARGB stroke_argb,
                                     CFX_PathData* pClippingPath,
                                     int nFlag) {
  CPDF_CharPosList CharPosList;
  CharPosList.Load(nChars, pCharCodes, pCharPos, pFont, font_size);
  if (CharPosList.m_nChars == 0)
    return true;
  bool bDraw = true;
  int32_t fontPosition = CharPosList.m_pCharPos[0].m_FallbackFontPosition;
  uint32_t startIndex = 0;
  for (uint32_t i = 0; i < CharPosList.m_nChars; i++) {
    int32_t curFontPosition = CharPosList.m_pCharPos[i].m_FallbackFontPosition;
    if (fontPosition == curFontPosition)
      continue;
    auto* font = fontPosition == -1
                     ? &pFont->m_Font
                     : pFont->m_FontFallbacks[fontPosition].get();
    if (!pDevice->DrawTextPath(i - startIndex,
                               CharPosList.m_pCharPos + startIndex, font,
                               font_size, pText2User, pUser2Device, pGraphState,
                               fill_argb, stroke_argb, pClippingPath, nFlag)) {
      bDraw = false;
    }
    fontPosition = curFontPosition;
    startIndex = i;
  }
  auto* font = fontPosition == -1 ? &pFont->m_Font
                                  : pFont->m_FontFallbacks[fontPosition].get();
  if (!pDevice->DrawTextPath(CharPosList.m_nChars - startIndex,
                             CharPosList.m_pCharPos + startIndex, font,
                             font_size, pText2User, pUser2Device, pGraphState,
                             fill_argb, stroke_argb, pClippingPath, nFlag)) {
    bDraw = false;
  }
  return bDraw;
}

// static
void CPDF_TextRenderer::DrawTextString(CFX_RenderDevice* pDevice,
                                       FX_FLOAT origin_x,
                                       FX_FLOAT origin_y,
                                       CPDF_Font* pFont,
                                       FX_FLOAT font_size,
                                       const CFX_Matrix* pMatrix,
                                       const CFX_ByteString& str,
                                       FX_ARGB fill_argb,
                                       FX_ARGB stroke_argb,
                                       const CFX_GraphStateData* pGraphState,
                                       const CPDF_RenderOptions* pOptions) {
  if (pFont->IsType3Font())
    return;

  int nChars = pFont->CountChar(str.c_str(), str.GetLength());
  if (nChars <= 0)
    return;

  int offset = 0;
  uint32_t* pCharCodes;
  FX_FLOAT* pCharPos;
  std::vector<uint32_t> codes;
  std::vector<FX_FLOAT> positions;
  if (nChars == 1) {
    pCharCodes = reinterpret_cast<uint32_t*>(
        pFont->GetNextChar(str.c_str(), str.GetLength(), offset));
    pCharPos = nullptr;
  } else {
    codes.resize(nChars);
    positions.resize(nChars - 1);
    FX_FLOAT cur_pos = 0;
    for (int i = 0; i < nChars; i++) {
      codes[i] = pFont->GetNextChar(str.c_str(), str.GetLength(), offset);
      if (i)
        positions[i - 1] = cur_pos;
      cur_pos += pFont->GetCharWidthF(codes[i]) * font_size / 1000;
    }
    pCharCodes = codes.data();
    pCharPos = positions.data();
  }
  CFX_Matrix matrix;
  if (pMatrix)
    matrix = *pMatrix;

  matrix.e = origin_x;
  matrix.f = origin_y;

  if (stroke_argb == 0) {
    DrawNormalText(pDevice, nChars, pCharCodes, pCharPos, pFont, font_size,
                   &matrix, fill_argb, pOptions);
  } else {
    DrawTextPath(pDevice, nChars, pCharCodes, pCharPos, pFont, font_size,
                 &matrix, nullptr, pGraphState, fill_argb, stroke_argb, nullptr,
                 0);
  }
}

// static
bool CPDF_TextRenderer::DrawNormalText(CFX_RenderDevice* pDevice,
                                       int nChars,
                                       uint32_t* pCharCodes,
                                       FX_FLOAT* pCharPos,
                                       CPDF_Font* pFont,
                                       FX_FLOAT font_size,
                                       const CFX_Matrix* pText2Device,
                                       FX_ARGB fill_argb,
                                       const CPDF_RenderOptions* pOptions) {
  CPDF_CharPosList CharPosList;
  CharPosList.Load(nChars, pCharCodes, pCharPos, pFont, font_size);
  if (CharPosList.m_nChars == 0)
    return true;
  int FXGE_flags = 0;
  if (pOptions) {
    uint32_t dwFlags = pOptions->m_Flags;
    if (dwFlags & RENDER_CLEARTYPE) {
      FXGE_flags |= FXTEXT_CLEARTYPE;
      if (dwFlags & RENDER_BGR_STRIPE) {
        FXGE_flags |= FXTEXT_BGR_STRIPE;
      }
    }
    if (dwFlags & RENDER_NOTEXTSMOOTH) {
      FXGE_flags |= FXTEXT_NOSMOOTH;
    }
    if (dwFlags & RENDER_PRINTGRAPHICTEXT) {
      FXGE_flags |= FXTEXT_PRINTGRAPHICTEXT;
    }
    if (dwFlags & RENDER_NO_NATIVETEXT) {
      FXGE_flags |= FXTEXT_NO_NATIVETEXT;
    }
    if (dwFlags & RENDER_PRINTIMAGETEXT) {
      FXGE_flags |= FXTEXT_PRINTIMAGETEXT;
    }
  } else {
    FXGE_flags = FXTEXT_CLEARTYPE;
  }
  if (pFont->IsCIDFont()) {
    FXGE_flags |= FXFONT_CIDFONT;
  }
  bool bDraw = true;
  int32_t fontPosition = CharPosList.m_pCharPos[0].m_FallbackFontPosition;
  uint32_t startIndex = 0;
  for (uint32_t i = 0; i < CharPosList.m_nChars; i++) {
    int32_t curFontPosition = CharPosList.m_pCharPos[i].m_FallbackFontPosition;
    if (fontPosition == curFontPosition)
      continue;
    auto* font = fontPosition == -1
                     ? &pFont->m_Font
                     : pFont->m_FontFallbacks[fontPosition].get();
    if (!pDevice->DrawNormalText(
            i - startIndex, CharPosList.m_pCharPos + startIndex, font,
            font_size, pText2Device, fill_argb, FXGE_flags)) {
      bDraw = false;
    }
    fontPosition = curFontPosition;
    startIndex = i;
  }
  auto* font = fontPosition == -1 ? &pFont->m_Font
                                  : pFont->m_FontFallbacks[fontPosition].get();
  if (!pDevice->DrawNormalText(CharPosList.m_nChars - startIndex,
                               CharPosList.m_pCharPos + startIndex, font,
                               font_size, pText2Device, fill_argb,
                               FXGE_flags)) {
    bDraw = false;
  }
  return bDraw;
}

void CPDF_RenderStatus::DrawTextPathWithPattern(const CPDF_TextObject* textobj,
                                                const CFX_Matrix* pObj2Device,
                                                CPDF_Font* pFont,
                                                FX_FLOAT font_size,
                                                const CFX_Matrix* pTextMatrix,
                                                bool bFill,
                                                bool bStroke) {
  if (!bStroke) {
    CPDF_PathObject path;
    std::vector<std::unique_ptr<CPDF_TextObject>> pCopy;
    pCopy.push_back(std::unique_ptr<CPDF_TextObject>(textobj->Clone()));
    path.m_bStroke = false;
    path.m_FillType = FXFILL_WINDING;
    path.m_ClipPath.AppendTexts(&pCopy);
    path.m_ColorState = textobj->m_ColorState;
    path.m_Path.AppendRect(textobj->m_Left, textobj->m_Bottom, textobj->m_Right,
                           textobj->m_Top);
    path.m_Left = textobj->m_Left;
    path.m_Bottom = textobj->m_Bottom;
    path.m_Right = textobj->m_Right;
    path.m_Top = textobj->m_Top;
    RenderSingleObject(&path, pObj2Device);
    return;
  }
  CPDF_CharPosList CharPosList;
  CharPosList.Load(textobj->m_nChars, textobj->m_pCharCodes,
                   textobj->m_pCharPos, pFont, font_size);
  for (uint32_t i = 0; i < CharPosList.m_nChars; i++) {
    FXTEXT_CHARPOS& charpos = CharPosList.m_pCharPos[i];
    auto font =
        charpos.m_FallbackFontPosition == -1
            ? &pFont->m_Font
            : pFont->m_FontFallbacks[charpos.m_FallbackFontPosition].get();
    const CFX_PathData* pPath =
        font->LoadGlyphPath(charpos.m_GlyphIndex, charpos.m_FontCharWidth);
    if (!pPath) {
      continue;
    }
    CPDF_PathObject path;
    path.m_GraphState = textobj->m_GraphState;
    path.m_ColorState = textobj->m_ColorState;
    CFX_Matrix matrix;
    if (charpos.m_bGlyphAdjust)
      matrix.Set(charpos.m_AdjustMatrix[0], charpos.m_AdjustMatrix[1],
                 charpos.m_AdjustMatrix[2], charpos.m_AdjustMatrix[3], 0, 0);
    matrix.Concat(font_size, 0, 0, font_size, charpos.m_OriginX,
                  charpos.m_OriginY);
    path.m_Path.Append(pPath, &matrix);
    path.m_Matrix = *pTextMatrix;
    path.m_bStroke = bStroke;
    path.m_FillType = bFill ? FXFILL_WINDING : 0;
    path.CalcBoundingBox();
    ProcessPath(&path, pObj2Device);
  }
}
