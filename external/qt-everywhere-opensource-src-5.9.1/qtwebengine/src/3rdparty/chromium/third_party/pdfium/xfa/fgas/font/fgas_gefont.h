// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FGAS_FONT_FGAS_GEFONT_H_
#define XFA_FGAS_FONT_FGAS_GEFONT_H_

#include <map>

#include "core/fxcrt/fx_memory.h"
#include "xfa/fgas/crt/fgas_utils.h"
#include "xfa/fgas/font/cfgas_fontmgr.h"

#define FXFONT_SUBST_ITALIC 0x02

class CFX_UnicodeEncoding;
class CXFA_PDFFontMgr;

class CFGAS_GEFont {
 public:
  static CFGAS_GEFont* LoadFont(const FX_WCHAR* pszFontFamily,
                                uint32_t dwFontStyles,
                                uint16_t wCodePage,
                                CFGAS_FontMgr* pFontMgr);
  static CFGAS_GEFont* LoadFont(CFX_Font* pExternalFont,
                                CFGAS_FontMgr* pFontMgr);
  static CFGAS_GEFont* LoadFont(std::unique_ptr<CFX_Font> pInternalFont,
                                CFGAS_FontMgr* pFontMgr);
#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
  static CFGAS_GEFont* LoadFont(const uint8_t* pBuffer,
                                int32_t iLength,
                                CFGAS_FontMgr* pFontMgr);
  static CFGAS_GEFont* LoadFont(IFX_Stream* pFontStream,
                                CFGAS_FontMgr* pFontMgr,
                                bool bSaveStream);
#endif

  ~CFGAS_GEFont();

  void Release();
  CFGAS_GEFont* Retain();
  CFGAS_GEFont* Derive(uint32_t dwFontStyles, uint16_t wCodePage = 0);
  void GetFamilyName(CFX_WideString& wsFamily) const;
  uint32_t GetFontStyles() const;
  bool GetCharWidth(FX_WCHAR wUnicode, int32_t& iWidth, bool bCharCode);
  int32_t GetGlyphIndex(FX_WCHAR wUnicode, bool bCharCode = false);
  int32_t GetAscent() const;
  int32_t GetDescent() const;
  bool GetCharBBox(FX_WCHAR wUnicode, CFX_Rect& bbox, bool bCharCode = false);
  bool GetBBox(CFX_Rect& bbox);
  int32_t GetItalicAngle() const;
  void Reset();
  CFGAS_GEFont* GetSubstFont(int32_t iGlyphIndex) const;
  CFX_Font* GetDevFont() const { return m_pFont; }
  void SetFontProvider(CXFA_PDFFontMgr* pProvider) { m_pProvider = pProvider; }
#if _FXM_PLATFORM_ != _FXM_PLATFORM_WINDOWS_
  void SetLogicalFontStyle(uint32_t dwLogFontStyle) {
    m_bUseLogFontStyle = true;
    m_dwLogFontStyle = dwLogFontStyle;
  }
#endif

 protected:
  explicit CFGAS_GEFont(CFGAS_FontMgr* pFontMgr);
  CFGAS_GEFont(CFGAS_GEFont* src, uint32_t dwFontStyles);

#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
  bool LoadFontInternal(const FX_WCHAR* pszFontFamily,
                        uint32_t dwFontStyles,
                        uint16_t wCodePage);
  bool LoadFontInternal(const uint8_t* pBuffer, int32_t length);
  bool LoadFontInternal(IFX_Stream* pFontStream, bool bSaveStream);
#endif
  bool LoadFontInternal(CFX_Font* pExternalFont);
  bool LoadFontInternal(std::unique_ptr<CFX_Font> pInternalFont);
  bool InitFont();
  bool GetCharBBoxInternal(FX_WCHAR wUnicode,
                           CFX_Rect& bbox,
                           bool bRecursive,
                           bool bCharCode = false);
  bool GetCharWidthInternal(FX_WCHAR wUnicode,
                            int32_t& iWidth,
                            bool bRecursive,
                            bool bCharCode);
  int32_t GetGlyphIndex(FX_WCHAR wUnicode,
                        bool bRecursive,
                        CFGAS_GEFont** ppFont,
                        bool bCharCode = false);

#if _FXM_PLATFORM_ != _FXM_PLATFORM_WINDOWS_
  bool m_bUseLogFontStyle;
  uint32_t m_dwLogFontStyle;
#endif
  CFX_Font* m_pFont;
  CFGAS_GEFont* const m_pSrcFont;
  CFGAS_FontMgr* const m_pFontMgr;
  int32_t m_iRefCount;
  bool m_bExternalFont;
  std::unique_ptr<IFX_Stream, ReleaseDeleter<IFX_Stream>> m_pStream;
  std::unique_ptr<IFX_SeekableReadStream,
                  ReleaseDeleter<IFX_SeekableReadStream>>
      m_pFileRead;
  std::unique_ptr<CFX_UnicodeEncoding> m_pFontEncoding;
  std::unique_ptr<CFX_DiscreteArrayTemplate<uint16_t>> m_pCharWidthMap;
  std::unique_ptr<CFX_MassArrayTemplate<CFX_Rect>> m_pRectArray;
  std::unique_ptr<CFX_MapPtrToPtr> m_pBBoxMap;
  CXFA_PDFFontMgr* m_pProvider;  // not owned.
  CFX_ArrayTemplate<CFGAS_GEFont*> m_SubstFonts;
  std::map<FX_WCHAR, CFGAS_GEFont*> m_FontMapper;
};

#endif  // XFA_FGAS_FONT_FGAS_GEFONT_H_
