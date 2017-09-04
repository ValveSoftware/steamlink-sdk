// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_FONT_CPDF_FONT_H_
#define CORE_FPDFAPI_FONT_CPDF_FONT_H_

#include <memory>
#include <vector>

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "core/fxge/fx_font.h"

#define PDFFONT_FIXEDPITCH 1
#define PDFFONT_SERIF 2
#define PDFFONT_SYMBOLIC 4
#define PDFFONT_SCRIPT 8
#define PDFFONT_NONSYMBOLIC 32
#define PDFFONT_ITALIC 64
#define PDFFONT_ALLCAP 0x10000
#define PDFFONT_SMALLCAP 0x20000
#define PDFFONT_FORCEBOLD 0x40000
#define PDFFONT_USEEXTERNATTR 0x80000

class CFX_SubstFont;
class CPDF_CIDFont;
class CPDF_Dictionary;
class CPDF_Document;
class CPDF_Object;
class CPDF_StreamAcc;
class CPDF_TrueTypeFont;
class CPDF_Type1Font;
class CPDF_Type3Font;
class CPDF_ToUnicodeMap;

class CPDF_Font {
 public:
  static std::unique_ptr<CPDF_Font> Create(CPDF_Document* pDoc,
                                           CPDF_Dictionary* pFontDict);
  static CPDF_Font* GetStockFont(CPDF_Document* pDoc,
                                 const CFX_ByteStringC& fontname);
  static const uint32_t kInvalidCharCode = static_cast<uint32_t>(-1);

  virtual ~CPDF_Font();

  virtual bool IsType1Font() const;
  virtual bool IsTrueTypeFont() const;
  virtual bool IsType3Font() const;
  virtual bool IsCIDFont() const;
  virtual const CPDF_Type1Font* AsType1Font() const;
  virtual CPDF_Type1Font* AsType1Font();
  virtual const CPDF_TrueTypeFont* AsTrueTypeFont() const;
  virtual CPDF_TrueTypeFont* AsTrueTypeFont();
  virtual const CPDF_Type3Font* AsType3Font() const;
  virtual CPDF_Type3Font* AsType3Font();
  virtual const CPDF_CIDFont* AsCIDFont() const;
  virtual CPDF_CIDFont* AsCIDFont();

  virtual bool IsVertWriting() const;
  virtual bool IsUnicodeCompatible() const;
  virtual uint32_t GetNextChar(const FX_CHAR* pString,
                               int nStrLen,
                               int& offset) const;
  virtual int CountChar(const FX_CHAR* pString, int size) const;
  virtual int AppendChar(FX_CHAR* buf, uint32_t charcode) const;
  virtual int GetCharSize(uint32_t charcode) const;
  virtual int GlyphFromCharCode(uint32_t charcode, bool* pVertGlyph) = 0;
  virtual int GlyphFromCharCodeExt(uint32_t charcode);
  virtual CFX_WideString UnicodeFromCharCode(uint32_t charcode) const;
  virtual uint32_t CharCodeFromUnicode(FX_WCHAR Unicode) const;

  const CFX_ByteString& GetBaseFont() const { return m_BaseFont; }
  CFX_SubstFont* GetSubstFont() const { return m_Font.GetSubstFont(); }
  uint32_t GetFlags() const { return m_Flags; }
  bool IsEmbedded() const { return IsType3Font() || m_pFontFile != nullptr; }
  CPDF_StreamAcc* GetFontFile() const { return m_pFontFile; }
  CPDF_Dictionary* GetFontDict() const { return m_pFontDict; }
  bool IsStandardFont() const;
  FXFT_Face GetFace() const { return m_Font.GetFace(); }
  void AppendChar(CFX_ByteString& str, uint32_t charcode) const;

  void GetFontBBox(FX_RECT& rect) const { rect = m_FontBBox; }
  int GetTypeAscent() const { return m_Ascent; }
  int GetTypeDescent() const { return m_Descent; }
  int GetItalicAngle() const { return m_ItalicAngle; }
  int GetStemV() const { return m_StemV; }
  int GetStringWidth(const FX_CHAR* pString, int size);
  uint32_t FallbackFontFromCharcode(uint32_t charcode);
  int FallbackGlyphFromCharcode(int fallbackFont, uint32_t charcode);

  virtual int GetCharWidthF(uint32_t charcode) = 0;
  virtual FX_RECT GetCharBBox(uint32_t charcode) = 0;

  CPDF_Document* m_pDocument;
  CFX_Font m_Font;
  std::vector<std::unique_ptr<CFX_Font>> m_FontFallbacks;

 protected:
  CPDF_Font();

  virtual bool Load() = 0;

  void LoadUnicodeMap() const;  // logically const only.
  void LoadPDFEncoding(CPDF_Object* pEncoding,
                       int& iBaseEncoding,
                       std::vector<CFX_ByteString>* pCharNames,
                       bool bEmbedded,
                       bool bTrueType);
  void LoadFontDescriptor(CPDF_Dictionary* pDict);
  void CheckFontMetrics();

  const FX_CHAR* GetAdobeCharName(int iBaseEncoding,
                                  const std::vector<CFX_ByteString>& charnames,
                                  int charcode);

  CFX_ByteString m_BaseFont;
  CPDF_StreamAcc* m_pFontFile;
  CPDF_Dictionary* m_pFontDict;
  mutable std::unique_ptr<CPDF_ToUnicodeMap> m_pToUnicodeMap;
  mutable bool m_bToUnicodeLoaded;
  int m_Flags;
  FX_RECT m_FontBBox;
  int m_StemV;
  int m_Ascent;
  int m_Descent;
  int m_ItalicAngle;
};

#endif  // CORE_FPDFAPI_FONT_CPDF_FONT_H_
