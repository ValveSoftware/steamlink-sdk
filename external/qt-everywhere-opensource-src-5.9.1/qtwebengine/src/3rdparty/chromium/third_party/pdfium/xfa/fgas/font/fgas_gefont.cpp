// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fgas/font/fgas_gefont.h"

#include <memory>
#include <utility>

#include "core/fxge/cfx_substfont.h"
#include "core/fxge/cfx_unicodeencoding.h"
#include "core/fxge/cfx_unicodeencodingex.h"
#include "xfa/fgas/crt/fgas_codepage.h"
#include "xfa/fgas/font/fgas_fontutils.h"
#include "xfa/fxfa/xfa_fontmgr.h"

// static
CFGAS_GEFont* CFGAS_GEFont::LoadFont(const FX_WCHAR* pszFontFamily,
                                     uint32_t dwFontStyles,
                                     uint16_t wCodePage,
                                     CFGAS_FontMgr* pFontMgr) {
#if _FXM_PLATFORM_ != _FXM_PLATFORM_WINDOWS_
  if (pFontMgr)
    return pFontMgr->GetFontByCodePage(wCodePage, dwFontStyles, pszFontFamily);
  return nullptr;
#else
  CFGAS_GEFont* pFont = new CFGAS_GEFont(pFontMgr);
  if (!pFont->LoadFontInternal(pszFontFamily, dwFontStyles, wCodePage)) {
    pFont->Release();
    return nullptr;
  }
  return pFont;
#endif
}

// static
CFGAS_GEFont* CFGAS_GEFont::LoadFont(CFX_Font* pExternalFont,
                                     CFGAS_FontMgr* pFontMgr) {
  CFGAS_GEFont* pFont = new CFGAS_GEFont(pFontMgr);
  if (!pFont->LoadFontInternal(pExternalFont)) {
    pFont->Release();
    return nullptr;
  }
  return pFont;
}

// static
CFGAS_GEFont* CFGAS_GEFont::LoadFont(std::unique_ptr<CFX_Font> pInternalFont,
                                     CFGAS_FontMgr* pFontMgr) {
  CFGAS_GEFont* pFont = new CFGAS_GEFont(pFontMgr);
  if (!pFont->LoadFontInternal(std::move(pInternalFont))) {
    pFont->Release();
    return nullptr;
  }
  return pFont;
}

#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
// static
CFGAS_GEFont* CFGAS_GEFont::LoadFont(const uint8_t* pBuffer,
                                     int32_t iLength,
                                     CFGAS_FontMgr* pFontMgr) {
  CFGAS_GEFont* pFont = new CFGAS_GEFont(pFontMgr);
  if (!pFont->LoadFontInternal(pBuffer, iLength)) {
    pFont->Release();
    return nullptr;
  }
  return pFont;
}

// static
CFGAS_GEFont* CFGAS_GEFont::LoadFont(IFX_Stream* pFontStream,
                                     CFGAS_FontMgr* pFontMgr,
                                     bool bSaveStream) {
  CFGAS_GEFont* pFont = new CFGAS_GEFont(pFontMgr);
  if (!pFont->LoadFontInternal(pFontStream, bSaveStream)) {
    pFont->Release();
    return nullptr;
  }
  return pFont;
}
#endif  // _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_

CFGAS_GEFont::CFGAS_GEFont(CFGAS_FontMgr* pFontMgr)
    :
#if _FXM_PLATFORM_ != _FXM_PLATFORM_WINDOWS_
      m_bUseLogFontStyle(false),
      m_dwLogFontStyle(0),
#endif
      m_pFont(nullptr),
      m_pSrcFont(nullptr),
      m_pFontMgr(pFontMgr),
      m_iRefCount(1),
      m_bExternalFont(false),
      m_pProvider(nullptr) {
}

CFGAS_GEFont::CFGAS_GEFont(CFGAS_GEFont* src, uint32_t dwFontStyles)
    :
#if _FXM_PLATFORM_ != _FXM_PLATFORM_WINDOWS_
      m_bUseLogFontStyle(false),
      m_dwLogFontStyle(0),
#endif
      m_pFont(nullptr),
      m_pSrcFont(src),
      m_pFontMgr(src->m_pFontMgr),
      m_iRefCount(1),
      m_bExternalFont(false),
      m_pProvider(nullptr) {
  ASSERT(m_pSrcFont->m_pFont);
  m_pSrcFont->Retain();
  m_pFont = new CFX_Font;
  m_pFont->LoadClone(m_pSrcFont->m_pFont);
  CFX_SubstFont* pSubst = m_pFont->GetSubstFont();
  if (!pSubst) {
    pSubst = new CFX_SubstFont;
    m_pFont->SetSubstFont(std::unique_ptr<CFX_SubstFont>(pSubst));
  }
  pSubst->m_Weight =
      (dwFontStyles & FX_FONTSTYLE_Bold) ? FXFONT_FW_BOLD : FXFONT_FW_NORMAL;
  if (dwFontStyles & FX_FONTSTYLE_Italic) {
    pSubst->m_SubstFlags |= FXFONT_SUBST_ITALIC;
  }
  InitFont();
}

CFGAS_GEFont::~CFGAS_GEFont() {
  for (int32_t i = 0; i < m_SubstFonts.GetSize(); i++)
    m_SubstFonts[i]->Release();

  m_SubstFonts.RemoveAll();
  m_FontMapper.clear();

  if (!m_bExternalFont)
    delete m_pFont;

  // If it is a shallow copy of another source font,
  // decrease the refcount of the source font.
  if (m_pSrcFont)
    m_pSrcFont->Release();
}

void CFGAS_GEFont::Release() {
  if (--m_iRefCount < 1) {
    if (m_pFontMgr) {
      m_pFontMgr->RemoveFont(this);
    }
    delete this;
  }
}
CFGAS_GEFont* CFGAS_GEFont::Retain() {
  ++m_iRefCount;
  return this;
}

#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
bool CFGAS_GEFont::LoadFontInternal(const FX_WCHAR* pszFontFamily,
                                    uint32_t dwFontStyles,
                                    uint16_t wCodePage) {
  if (m_pFont) {
    return false;
  }
  CFX_ByteString csFontFamily;
  if (pszFontFamily) {
    csFontFamily = CFX_ByteString::FromUnicode(pszFontFamily);
  }
  uint32_t dwFlags = 0;
  if (dwFontStyles & FX_FONTSTYLE_FixedPitch) {
    dwFlags |= FXFONT_FIXED_PITCH;
  }
  if (dwFontStyles & FX_FONTSTYLE_Serif) {
    dwFlags |= FXFONT_SERIF;
  }
  if (dwFontStyles & FX_FONTSTYLE_Symbolic) {
    dwFlags |= FXFONT_SYMBOLIC;
  }
  if (dwFontStyles & FX_FONTSTYLE_Script) {
    dwFlags |= FXFONT_SCRIPT;
  }
  if (dwFontStyles & FX_FONTSTYLE_Italic) {
    dwFlags |= FXFONT_ITALIC;
  }
  if (dwFontStyles & FX_FONTSTYLE_Bold) {
    dwFlags |= FXFONT_BOLD;
  }
  if (dwFontStyles & FX_FONTSTYLE_ExactMatch) {
    dwFlags |= FXFONT_EXACTMATCH;
  }
  int32_t iWeight =
      (dwFontStyles & FX_FONTSTYLE_Bold) ? FXFONT_FW_BOLD : FXFONT_FW_NORMAL;
  m_pFont = new CFX_Font;
  if ((dwFlags & FXFONT_ITALIC) && (dwFlags & FXFONT_BOLD)) {
    csFontFamily += ",BoldItalic";
  } else if (dwFlags & FXFONT_BOLD) {
    csFontFamily += ",Bold";
  } else if (dwFlags & FXFONT_ITALIC) {
    csFontFamily += ",Italic";
  }
  m_pFont->LoadSubst(csFontFamily, true, dwFlags, iWeight, 0, wCodePage, false);
  if (!m_pFont->GetFace())
    return false;
  return InitFont();
}

bool CFGAS_GEFont::LoadFontInternal(const uint8_t* pBuffer, int32_t length) {
  if (m_pFont)
    return false;

  m_pFont = new CFX_Font;
  if (!m_pFont->LoadEmbedded(pBuffer, length))
    return false;
  return InitFont();
}

bool CFGAS_GEFont::LoadFontInternal(IFX_Stream* pFontStream, bool bSaveStream) {
  if (m_pFont || m_pFileRead || !pFontStream || pFontStream->GetLength() < 1)
    return false;
  if (bSaveStream)
    m_pStream.reset(pFontStream);

  m_pFileRead.reset(FX_CreateFileRead(pFontStream, false));
  m_pFont = new CFX_Font;
  if (m_pFont->LoadFile(m_pFileRead.get()))
    return InitFont();
  m_pFileRead.reset();
  return false;
}
#endif  // _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_

bool CFGAS_GEFont::LoadFontInternal(CFX_Font* pExternalFont) {
  if (m_pFont || !pExternalFont)
    return false;

  m_pFont = pExternalFont;
  m_bExternalFont = true;
  return InitFont();
}

bool CFGAS_GEFont::LoadFontInternal(std::unique_ptr<CFX_Font> pInternalFont) {
  if (m_pFont || !pInternalFont)
    return false;

  m_pFont = pInternalFont.release();
  m_bExternalFont = false;
  return InitFont();
}

bool CFGAS_GEFont::InitFont() {
  if (!m_pFont)
    return false;
  if (!m_pFontEncoding) {
    m_pFontEncoding.reset(FX_CreateFontEncodingEx(m_pFont));
    if (!m_pFontEncoding)
      return false;
  }
  if (!m_pCharWidthMap)
    m_pCharWidthMap.reset(new CFX_DiscreteArrayTemplate<uint16_t>(1024));
  if (!m_pRectArray)
    m_pRectArray.reset(new CFX_MassArrayTemplate<CFX_Rect>(16));
  if (!m_pBBoxMap)
    m_pBBoxMap.reset(new CFX_MapPtrToPtr(16));

  return true;
}

CFGAS_GEFont* CFGAS_GEFont::Derive(uint32_t dwFontStyles, uint16_t wCodePage) {
  if (GetFontStyles() == dwFontStyles)
    return Retain();
  return new CFGAS_GEFont(this, dwFontStyles);
}

void CFGAS_GEFont::GetFamilyName(CFX_WideString& wsFamily) const {
  if (!m_pFont->GetSubstFont() ||
      m_pFont->GetSubstFont()->m_Family.GetLength() == 0) {
    wsFamily = CFX_WideString::FromLocal(m_pFont->GetFamilyName().AsStringC());
  } else {
    wsFamily = CFX_WideString::FromLocal(
        m_pFont->GetSubstFont()->m_Family.AsStringC());
  }
}

uint32_t CFGAS_GEFont::GetFontStyles() const {
  ASSERT(m_pFont);
#if _FXM_PLATFORM_ != _FXM_PLATFORM_WINDOWS_
  if (m_bUseLogFontStyle)
    return m_dwLogFontStyle;
#endif

  uint32_t dwStyles = 0;
  auto* pSubstFont = m_pFont->GetSubstFont();
  if (pSubstFont) {
    if (pSubstFont->m_Weight == FXFONT_FW_BOLD)
      dwStyles |= FX_FONTSTYLE_Bold;
    if (pSubstFont->m_SubstFlags & FXFONT_SUBST_ITALIC)
      dwStyles |= FX_FONTSTYLE_Italic;
  } else {
    if (m_pFont->IsBold())
      dwStyles |= FX_FONTSTYLE_Bold;
    if (m_pFont->IsItalic())
      dwStyles |= FX_FONTSTYLE_Italic;
  }
  return dwStyles;
}

bool CFGAS_GEFont::GetCharWidth(FX_WCHAR wUnicode,
                                int32_t& iWidth,
                                bool bCharCode) {
  return GetCharWidthInternal(wUnicode, iWidth, true, bCharCode);
}

bool CFGAS_GEFont::GetCharWidthInternal(FX_WCHAR wUnicode,
                                        int32_t& iWidth,
                                        bool bRecursive,
                                        bool bCharCode) {
  ASSERT(m_pCharWidthMap);
  iWidth = m_pCharWidthMap->GetAt(wUnicode, 0);
  if (iWidth == 65535)
    return false;

  if (iWidth > 0)
    return true;

  if (!m_pProvider ||
      !m_pProvider->GetCharWidth(this, wUnicode, bCharCode, &iWidth)) {
    CFGAS_GEFont* pFont = nullptr;
    int32_t iGlyph = GetGlyphIndex(wUnicode, true, &pFont, bCharCode);
    if (iGlyph != 0xFFFF && pFont) {
      if (pFont == this) {
        iWidth = m_pFont->GetGlyphWidth(iGlyph);
        if (iWidth < 0) {
          iWidth = -1;
        }
      } else if (pFont->GetCharWidthInternal(wUnicode, iWidth, false,
                                             bCharCode)) {
        return true;
      }
    } else {
      iWidth = -1;
    }
  }
  m_pCharWidthMap->SetAtGrow(wUnicode, iWidth);
  return iWidth > 0;
}

bool CFGAS_GEFont::GetCharBBox(FX_WCHAR wUnicode,
                               CFX_Rect& bbox,
                               bool bCharCode) {
  return GetCharBBoxInternal(wUnicode, bbox, true, bCharCode);
}

bool CFGAS_GEFont::GetCharBBoxInternal(FX_WCHAR wUnicode,
                                       CFX_Rect& bbox,
                                       bool bRecursive,
                                       bool bCharCode) {
  ASSERT(m_pRectArray);
  ASSERT(m_pBBoxMap);
  void* pRect = nullptr;
  if (!m_pBBoxMap->Lookup((void*)(uintptr_t)wUnicode, pRect)) {
    CFGAS_GEFont* pFont = nullptr;
    int32_t iGlyph = GetGlyphIndex(wUnicode, true, &pFont, bCharCode);
    if (iGlyph != 0xFFFF && pFont) {
      if (pFont == this) {
        FX_RECT rtBBox;
        if (m_pFont->GetGlyphBBox(iGlyph, rtBBox)) {
          CFX_Rect rt;
          rt.Set(rtBBox.left, rtBBox.top, rtBBox.Width(), rtBBox.Height());
          int32_t index = m_pRectArray->Add(rt);
          pRect = m_pRectArray->GetPtrAt(index);
          m_pBBoxMap->SetAt((void*)(uintptr_t)wUnicode, pRect);
        }
      } else if (pFont->GetCharBBoxInternal(wUnicode, bbox, false, bCharCode)) {
        return true;
      }
    }
  }
  if (!pRect)
    return false;

  bbox = *static_cast<const CFX_Rect*>(pRect);
  return true;
}
bool CFGAS_GEFont::GetBBox(CFX_Rect& bbox) {
  FX_RECT rt(0, 0, 0, 0);
  bool bRet = m_pFont->GetBBox(rt);
  if (bRet) {
    bbox.left = rt.left;
    bbox.width = rt.Width();
    bbox.top = rt.bottom;
    bbox.height = -rt.Height();
  }
  return bRet;
}
int32_t CFGAS_GEFont::GetItalicAngle() const {
  if (!m_pFont->GetSubstFont()) {
    return 0;
  }
  return m_pFont->GetSubstFont()->m_ItalicAngle;
}
int32_t CFGAS_GEFont::GetGlyphIndex(FX_WCHAR wUnicode, bool bCharCode) {
  return GetGlyphIndex(wUnicode, true, nullptr, bCharCode);
}
int32_t CFGAS_GEFont::GetGlyphIndex(FX_WCHAR wUnicode,
                                    bool bRecursive,
                                    CFGAS_GEFont** ppFont,
                                    bool bCharCode) {
  ASSERT(m_pFontEncoding);
  int32_t iGlyphIndex = m_pFontEncoding->GlyphFromCharCode(wUnicode);
  if (iGlyphIndex > 0) {
    if (ppFont) {
      *ppFont = this;
    }
    return iGlyphIndex;
  }
  const FGAS_FONTUSB* pFontUSB = FGAS_GetUnicodeBitField(wUnicode);
  if (!pFontUSB) {
    return 0xFFFF;
  }
  uint16_t wBitField = pFontUSB->wBitField;
  if (wBitField >= 128) {
    return 0xFFFF;
  }
  auto it = m_FontMapper.find(wUnicode);
  CFGAS_GEFont* pFont = it != m_FontMapper.end() ? it->second : nullptr;
  if (pFont && pFont != this) {
    iGlyphIndex = pFont->GetGlyphIndex(wUnicode, false, nullptr, bCharCode);
    if (iGlyphIndex != 0xFFFF) {
      int32_t i = m_SubstFonts.Find(pFont);
      if (i > -1) {
        iGlyphIndex |= ((i + 1) << 24);
        if (ppFont)
          *ppFont = pFont;
        return iGlyphIndex;
      }
    }
  }
  if (m_pFontMgr && bRecursive) {
    CFX_WideString wsFamily;
    GetFamilyName(wsFamily);
#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
    CFGAS_GEFont* pFont = m_pFontMgr->GetDefFontByUnicode(
        wUnicode, GetFontStyles(), wsFamily.c_str());
#else
    CFGAS_GEFont* pFont = m_pFontMgr->GetFontByUnicode(
        wUnicode, GetFontStyles(), wsFamily.c_str());
    if (!pFont)
      pFont = m_pFontMgr->GetFontByUnicode(wUnicode, GetFontStyles(), nullptr);
#endif
    if (pFont) {
      if (pFont == this) {
        pFont->Release();
        return 0xFFFF;
      }
      m_FontMapper[wUnicode] = pFont;
      int32_t i = m_SubstFonts.GetSize();
      m_SubstFonts.Add(pFont);
      iGlyphIndex = pFont->GetGlyphIndex(wUnicode, false, nullptr, bCharCode);
      if (iGlyphIndex != 0xFFFF) {
        iGlyphIndex |= ((i + 1) << 24);
        if (ppFont)
          *ppFont = pFont;
        return iGlyphIndex;
      }
    }
  }
  return 0xFFFF;
}
int32_t CFGAS_GEFont::GetAscent() const {
  return m_pFont->GetAscent();
}
int32_t CFGAS_GEFont::GetDescent() const {
  return m_pFont->GetDescent();
}
void CFGAS_GEFont::Reset() {
  for (int32_t i = 0; i < m_SubstFonts.GetSize(); i++)
    m_SubstFonts[i]->Reset();
  if (m_pCharWidthMap) {
    m_pCharWidthMap->RemoveAll();
  }
  if (m_pBBoxMap) {
    m_pBBoxMap->RemoveAll();
  }
  if (m_pRectArray) {
    m_pRectArray->RemoveAll(false);
  }
}
CFGAS_GEFont* CFGAS_GEFont::GetSubstFont(int32_t iGlyphIndex) const {
  iGlyphIndex = ((uint32_t)iGlyphIndex) >> 24;
  return iGlyphIndex == 0 ? const_cast<CFGAS_GEFont*>(this)
                          : m_SubstFonts[iGlyphIndex - 1];
}
