// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxge/apple/apple_int.h"
#include "core/fxge/include/fx_ge.h"

#if _FX_OS_ == _FX_MACOSX_
static const struct {
  const FX_CHAR* m_pName;
  const FX_CHAR* m_pSubstName;
} Base14Substs[] = {
    {"Courier", "Courier New"},
    {"Courier-Bold", "Courier New Bold"},
    {"Courier-BoldOblique", "Courier New Bold Italic"},
    {"Courier-Oblique", "Courier New Italic"},
    {"Helvetica", "Arial"},
    {"Helvetica-Bold", "Arial Bold"},
    {"Helvetica-BoldOblique", "Arial Bold Italic"},
    {"Helvetica-Oblique", "Arial Italic"},
    {"Times-Roman", "Times New Roman"},
    {"Times-Bold", "Times New Roman Bold"},
    {"Times-BoldItalic", "Times New Roman Bold Italic"},
    {"Times-Italic", "Times New Roman Italic"},
};

class CFX_MacFontInfo : public CFX_FolderFontInfo {
 public:
  CFX_MacFontInfo() {}
  ~CFX_MacFontInfo() override {}

  // CFX_FolderFontInfo
  void* MapFont(int weight,
                FX_BOOL bItalic,
                int charset,
                int pitch_family,
                const FX_CHAR* family,
                int& iExact) override;
};

#define JAPAN_GOTHIC "Hiragino Kaku Gothic Pro W6"
#define JAPAN_MINCHO "Hiragino Mincho Pro W6"
static void GetJapanesePreference(CFX_ByteString& face,
                                  int weight,
                                  int pitch_family) {
  if (face.Find("Gothic") >= 0) {
    face = JAPAN_GOTHIC;
    return;
  }
  if (!(pitch_family & FXFONT_FF_ROMAN) && weight > 400) {
    face = JAPAN_GOTHIC;
  } else {
    face = JAPAN_MINCHO;
  }
}
void* CFX_MacFontInfo::MapFont(int weight,
                               FX_BOOL bItalic,
                               int charset,
                               int pitch_family,
                               const FX_CHAR* cstr_face,
                               int& iExact) {
  CFX_ByteString face = cstr_face;
  int iBaseFont;
  for (iBaseFont = 0; iBaseFont < 12; iBaseFont++)
    if (face == CFX_ByteStringC(Base14Substs[iBaseFont].m_pName)) {
      face = Base14Substs[iBaseFont].m_pSubstName;
      iExact = TRUE;
      break;
    }
  if (iBaseFont < 12) {
    return GetFont(face.c_str());
  }
  auto it = m_FontList.find(face);
  if (it != m_FontList.end())
    return it->second;

  if (charset == FXFONT_ANSI_CHARSET && (pitch_family & FXFONT_FF_FIXEDPITCH)) {
    return GetFont("Courier New");
  }
  if (charset == FXFONT_ANSI_CHARSET || charset == FXFONT_SYMBOL_CHARSET) {
    return nullptr;
  }
  switch (charset) {
    case FXFONT_SHIFTJIS_CHARSET:
      GetJapanesePreference(face, weight, pitch_family);
      break;
    case FXFONT_GB2312_CHARSET:
      face = "STSong";
      break;
    case FXFONT_HANGEUL_CHARSET:
      face = "AppleMyungjo";
      break;
    case FXFONT_CHINESEBIG5_CHARSET:
      face = "LiSong Pro Light";
  }
  it = m_FontList.find(face);
  return it != m_FontList.end() ? it->second : nullptr;
}

std::unique_ptr<IFX_SystemFontInfo> IFX_SystemFontInfo::CreateDefault(
    const char** pUnused) {
  CFX_MacFontInfo* pInfo(new CFX_MacFontInfo);
  pInfo->AddPath("~/Library/Fonts");
  pInfo->AddPath("/Library/Fonts");
  pInfo->AddPath("/System/Library/Fonts");
  return std::unique_ptr<CFX_MacFontInfo>(pInfo);
}

void CFX_GEModule::InitPlatform() {
  m_pPlatformData = new CApplePlatform;
  m_pFontMgr->SetSystemFontInfo(IFX_SystemFontInfo::CreateDefault(nullptr));
}
void CFX_GEModule::DestroyPlatform() {
  delete (CApplePlatform*)m_pPlatformData;
  m_pPlatformData = nullptr;
}
#endif
