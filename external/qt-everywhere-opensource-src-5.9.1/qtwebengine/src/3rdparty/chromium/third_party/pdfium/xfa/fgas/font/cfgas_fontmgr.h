// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FGAS_FONT_CFGAS_FONTMGR_H_
#define XFA_FGAS_FONT_CFGAS_FONTMGR_H_

#include <vector>

#include "core/fxcrt/fx_ext.h"
#include "core/fxge/cfx_fontmapper.h"
#include "core/fxge/fx_freetype.h"
#include "core/fxge/ifx_systemfontinfo.h"
#include "third_party/freetype/include/freetype/fttypes.h"
#include "xfa/fgas/crt/fgas_stream.h"
#include "xfa/fgas/font/cfgas_fontmgr.h"

#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
#include "xfa/fgas/crt/fgas_memory.h"
#include "xfa/fgas/crt/fgas_utils.h"
#endif  // _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_

#define FX_FONTSTYLE_Normal 0x00
#define FX_FONTSTYLE_FixedPitch 0x01
#define FX_FONTSTYLE_Serif 0x02
#define FX_FONTSTYLE_Symbolic 0x04
#define FX_FONTSTYLE_Script 0x08
#define FX_FONTSTYLE_Italic 0x40
#define FX_FONTSTYLE_Bold 0x40000
#define FX_FONTSTYLE_BoldItalic (FX_FONTSTYLE_Bold | FX_FONTSTYLE_Italic)
#define FX_FONTSTYLE_ExactMatch 0x80000000

class CFX_FontSourceEnum_File;
class CFGAS_GEFont;
class CXFA_PDFFontMgr;
class CFGAS_FontMgr;

#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
#define FX_FONTMATCHPARA_MatchStyle 0x01

struct FX_FONTMATCHPARAMS {
  const FX_WCHAR* pwsFamily;
  uint32_t dwFontStyles;
  uint32_t dwUSB;
  uint32_t dwMatchFlags;
  FX_WCHAR wUnicode;
  uint16_t wCodePage;
};

typedef FX_FONTMATCHPARAMS* FX_LPFONTMATCHPARAMS;

struct FX_FONTSIGNATURE {
  uint32_t fsUsb[4];
  uint32_t fsCsb[2];
};

inline bool operator==(const FX_FONTSIGNATURE& left,
                       const FX_FONTSIGNATURE& right) {
  return left.fsUsb[0] == right.fsUsb[0] && left.fsUsb[1] == right.fsUsb[1] &&
         left.fsUsb[2] == right.fsUsb[2] && left.fsUsb[3] == right.fsUsb[3] &&
         left.fsCsb[0] == right.fsCsb[0] && left.fsCsb[1] == right.fsCsb[1];
}

struct FX_FONTDESCRIPTOR {
  FX_WCHAR wsFontFace[32];
  uint32_t dwFontStyles;
  uint8_t uCharSet;
  FX_FONTSIGNATURE FontSignature;
};

typedef CFX_MassArrayTemplate<FX_FONTDESCRIPTOR> CFX_FontDescriptors;

inline bool operator==(const FX_FONTDESCRIPTOR& left,
                       const FX_FONTDESCRIPTOR& right) {
  return left.uCharSet == right.uCharSet &&
         left.dwFontStyles == right.dwFontStyles &&
         left.FontSignature == right.FontSignature &&
         FXSYS_wcscmp(left.wsFontFace, right.wsFontFace) == 0;
}

typedef void (*FX_LPEnumAllFonts)(CFX_FontDescriptors& fonts,
                                  const FX_WCHAR* pwsFaceName,
                                  FX_WCHAR wUnicode);

FX_LPEnumAllFonts FX_GetDefFontEnumerator();

class CFGAS_FontMgr {
 public:
  explicit CFGAS_FontMgr(FX_LPEnumAllFonts pEnumerator);
  ~CFGAS_FontMgr();

  static std::unique_ptr<CFGAS_FontMgr> Create(FX_LPEnumAllFonts pEnumerator);

  CFGAS_GEFont* GetDefFontByCodePage(uint16_t wCodePage,
                                     uint32_t dwFontStyles,
                                     const FX_WCHAR* pszFontFamily = nullptr);
  CFGAS_GEFont* GetDefFontByUnicode(FX_WCHAR wUnicode,
                                    uint32_t dwFontStyles,
                                    const FX_WCHAR* pszFontFamily = nullptr);
  CFGAS_GEFont* LoadFont(const FX_WCHAR* pszFontFamily,
                         uint32_t dwFontStyles,
                         uint16_t wCodePage = 0xFFFF);
  CFGAS_GEFont* LoadFont(const uint8_t* pBuffer, int32_t iLength);
  CFGAS_GEFont* LoadFont(IFX_Stream* pFontStream,
                         const FX_WCHAR* pszFontAlias = nullptr,
                         uint32_t dwFontStyles = 0,
                         uint16_t wCodePage = 0,
                         bool bSaveStream = false);
  CFGAS_GEFont* LoadFont(CFGAS_GEFont* pSrcFont,
                         uint32_t dwFontStyles,
                         uint16_t wCodePage = 0xFFFF);

  // TODO(npm): This method is not being used, but probably should be in
  // destructor
  void ClearFontCache();
  void RemoveFont(CFGAS_GEFont* pFont);

 private:
  void RemoveFont(CFX_MapPtrToPtr& fontMap, CFGAS_GEFont* pFont);
  FX_FONTDESCRIPTOR const* FindFont(const FX_WCHAR* pszFontFamily,
                                    uint32_t dwFontStyles,
                                    uint32_t dwMatchFlags,
                                    uint16_t wCodePage,
                                    uint32_t dwUSB = 999,
                                    FX_WCHAR wUnicode = 0);

  FX_LPEnumAllFonts m_pEnumerator;
  CFX_FontDescriptors m_FontFaces;
  CFX_ArrayTemplate<CFGAS_GEFont*> m_Fonts;
  CFX_MapPtrToPtr m_CPFonts;
  CFX_MapPtrToPtr m_FamilyFonts;
  CFX_MapPtrToPtr m_UnicodeFonts;
  CFX_MapPtrToPtr m_BufferFonts;
  CFX_MapPtrToPtr m_StreamFonts;
  CFX_MapPtrToPtr m_DeriveFonts;
};

#else  // _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
class CFX_FontDescriptor {
 public:
  CFX_FontDescriptor();
  ~CFX_FontDescriptor();

  int32_t m_nFaceIndex;
  CFX_WideString m_wsFaceName;
  CFX_WideStringArray m_wsFamilyNames;
  uint32_t m_dwFontStyles;
  uint32_t m_dwUsb[4];
  uint32_t m_dwCsb[2];
};

typedef CFX_ArrayTemplate<CFX_FontDescriptor*> CFX_FontDescriptors;

struct FX_FontDescriptorInfo {
 public:
  CFX_FontDescriptor* pFont;
  int32_t nPenalty;

  bool operator>(const FX_FontDescriptorInfo& other) const {
    return nPenalty > other.nPenalty;
  }
  bool operator<(const FX_FontDescriptorInfo& other) const {
    return nPenalty < other.nPenalty;
  }
  bool operator==(const FX_FontDescriptorInfo& other) const {
    return nPenalty == other.nPenalty;
  }
};

typedef CFX_ArrayTemplate<FX_FontDescriptorInfo> CFX_FontDescriptorInfos;

struct FX_HandleParentPath {
  FX_HandleParentPath() {}
  FX_HandleParentPath(const FX_HandleParentPath& x) {
    pFileHandle = x.pFileHandle;
    bsParentPath = x.bsParentPath;
  }
  FX_FileHandle* pFileHandle;
  CFX_ByteString bsParentPath;
};

class CFX_FontSourceEnum_File {
 public:
  CFX_FontSourceEnum_File();
  ~CFX_FontSourceEnum_File();

  FX_POSITION GetStartPosition();
  IFX_FileAccess* GetNext(FX_POSITION& pos);

 private:
  CFX_ByteString GetNextFile();

  CFX_WideString m_wsNext;
  CFX_ObjectArray<FX_HandleParentPath> m_FolderQueue;
  CFX_ByteStringArray m_FolderPaths;
};

class CFGAS_FontMgr {
 public:
  explicit CFGAS_FontMgr(CFX_FontSourceEnum_File* pFontEnum);
  ~CFGAS_FontMgr();
  static std::unique_ptr<CFGAS_FontMgr> Create(
      CFX_FontSourceEnum_File* pFontEnum);

  CFGAS_GEFont* GetFontByCodePage(uint16_t wCodePage,
                                  uint32_t dwFontStyles,
                                  const FX_WCHAR* pszFontFamily);
  CFGAS_GEFont* GetFontByUnicode(FX_WCHAR wUnicode,
                                 uint32_t dwFontStyles,
                                 const FX_WCHAR* pszFontFamily);
  void ClearFontCache();
  void RemoveFont(CFGAS_GEFont* pFont);

  CFGAS_GEFont* LoadFont(const CFX_WideString& wsFaceName,
                         int32_t iFaceIndex,
                         int32_t* pFaceCount);
  inline CFGAS_GEFont* LoadFont(const FX_WCHAR* pszFontFamily,
                                uint32_t dwFontStyles,
                                uint16_t wCodePage) {
    return GetFontByCodePage(wCodePage, dwFontStyles, pszFontFamily);
  }
  bool EnumFonts();
  bool EnumFontsFromFontMapper();
  bool EnumFontsFromFiles();

 private:
  void RegisterFace(FXFT_Face pFace, const CFX_WideString* pFaceName);
  void RegisterFaces(IFX_SeekableReadStream* pFontStream,
                     const CFX_WideString* pFaceName);
  void GetNames(const uint8_t* name_table, CFX_WideStringArray& Names);
  std::vector<uint16_t> GetCharsets(FXFT_Face pFace) const;
  void GetUSBCSB(FXFT_Face pFace, uint32_t* USB, uint32_t* CSB);
  uint32_t GetFlags(FXFT_Face pFace);
  bool VerifyUnicode(CFX_FontDescriptor* pDesc, FX_WCHAR wcUnicode);
  bool VerifyUnicode(CFGAS_GEFont* pFont, FX_WCHAR wcUnicode);
  int32_t IsPartName(const CFX_WideString& Name1, const CFX_WideString& Name2);
  int32_t MatchFonts(CFX_FontDescriptorInfos& MatchedFonts,
                     uint16_t wCodePage,
                     uint32_t dwFontStyles,
                     const CFX_WideString& FontName,
                     FX_WCHAR wcUnicode = 0xFFFE);
  int32_t CalcPenalty(CFX_FontDescriptor* pInstalled,
                      uint16_t wCodePage,
                      uint32_t dwFontStyles,
                      const CFX_WideString& FontName,
                      FX_WCHAR wcUnicode = 0xFFFE);
  FXFT_Face LoadFace(IFX_SeekableReadStream* pFontStream, int32_t iFaceIndex);
  IFX_SeekableReadStream* CreateFontStream(CFX_FontMapper* pFontMapper,
                                           IFX_SystemFontInfo* pSystemFontInfo,
                                           uint32_t index);
  IFX_SeekableReadStream* CreateFontStream(const CFX_ByteString& bsFaceName);

  CFX_FontDescriptors m_InstalledFonts;
  CFX_MapPtrTemplate<uint32_t, CFX_FontDescriptorInfos*> m_Hash2CandidateList;
  CFX_MapPtrTemplate<uint32_t, CFX_ArrayTemplate<CFGAS_GEFont*>*> m_Hash2Fonts;
  CFX_MapPtrTemplate<CFGAS_GEFont*, IFX_SeekableReadStream*> m_IFXFont2FileRead;
  CFX_MapPtrTemplate<FX_WCHAR, CFGAS_GEFont*> m_FailedUnicodes2Nullptr;
  CFX_FontSourceEnum_File* const m_pFontSource;
};
#endif

#endif  // XFA_FGAS_FONT_CFGAS_FONTMGR_H_
