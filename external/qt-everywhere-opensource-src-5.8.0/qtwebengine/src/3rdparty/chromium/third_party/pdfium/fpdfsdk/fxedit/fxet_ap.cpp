// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/fpdf_font/include/cpdf_font.h"
#include "core/fpdfapi/fpdf_parser/include/fpdf_parser_decode.h"
#include "core/fpdfdoc/include/cpvt_word.h"
#include "core/fpdfdoc/include/ipvt_fontmap.h"
#include "fpdfsdk/fxedit/include/fx_edit.h"
#include "fpdfsdk/fxedit/include/fxet_edit.h"

namespace {

CFX_ByteString GetWordRenderString(const CFX_ByteString& strWords) {
  if (strWords.GetLength() > 0)
    return PDF_EncodeString(strWords) + " Tj\n";
  return CFX_ByteString();
}

CFX_ByteString GetFontSetString(IPVT_FontMap* pFontMap,
                                int32_t nFontIndex,
                                FX_FLOAT fFontSize) {
  if (!pFontMap)
    return CFX_ByteString();

  CFX_ByteString sFontAlias = pFontMap->GetPDFFontAlias(nFontIndex);
  if (sFontAlias.GetLength() <= 0 || fFontSize <= 0)
    return CFX_ByteString();

  CFX_ByteTextBuf sRet;
  sRet << "/" << sFontAlias << " " << fFontSize << " Tf\n";
  return sRet.MakeString();
}

}  // namespace

CFX_ByteString GetPDFWordString(IPVT_FontMap* pFontMap,
                                int32_t nFontIndex,
                                uint16_t Word,
                                uint16_t SubWord) {
  CPDF_Font* pPDFFont = pFontMap->GetPDFFont(nFontIndex);
  if (!pPDFFont)
    return CFX_ByteString();

  CFX_ByteString sWord;
  if (SubWord > 0) {
    Word = SubWord;
  } else {
    uint32_t dwCharCode = pPDFFont->IsUnicodeCompatible()
                              ? pPDFFont->CharCodeFromUnicode(Word)
                              : pFontMap->CharCodeFromUnicode(nFontIndex, Word);

    if (dwCharCode > 0) {
      pPDFFont->AppendChar(sWord, dwCharCode);
      return sWord;
    }
  }

  pPDFFont->AppendChar(sWord, Word);
  return sWord;
}

CFX_ByteString IFX_Edit::GetEditAppearanceStream(IFX_Edit* pEdit,
                                                 const CFX_FloatPoint& ptOffset,
                                                 const CPVT_WordRange* pRange,
                                                 FX_BOOL bContinuous,
                                                 uint16_t SubWord) {
  IFX_Edit_Iterator* pIterator = pEdit->GetIterator();
  if (pRange)
    pIterator->SetAt(pRange->BeginPos);
  else
    pIterator->SetAt(0);

  CFX_ByteTextBuf sEditStream;
  CFX_ByteTextBuf sWords;
  int32_t nCurFontIndex = -1;
  CFX_FloatPoint ptOld(0.0f, 0.0f);
  CFX_FloatPoint ptNew(0.0f, 0.0f);
  CPVT_WordPlace oldplace;
  while (pIterator->NextWord()) {
    CPVT_WordPlace place = pIterator->GetAt();

    if (pRange && place.WordCmp(pRange->EndPos) > 0)
      break;

    if (bContinuous) {
      if (place.LineCmp(oldplace) != 0) {
        if (sWords.GetSize() > 0) {
          sEditStream << GetWordRenderString(sWords.MakeString());
          sWords.Clear();
        }

        CPVT_Word word;
        if (pIterator->GetWord(word)) {
          ptNew = CFX_FloatPoint(word.ptWord.x + ptOffset.x,
                                 word.ptWord.y + ptOffset.y);
        } else {
          CPVT_Line line;
          pIterator->GetLine(line);
          ptNew = CFX_FloatPoint(line.ptLine.x + ptOffset.x,
                                 line.ptLine.y + ptOffset.y);
        }

        if (ptNew.x != ptOld.x || ptNew.y != ptOld.y) {
          sEditStream << ptNew.x - ptOld.x << " " << ptNew.y - ptOld.y
                      << " Td\n";

          ptOld = ptNew;
        }
      }

      CPVT_Word word;
      if (pIterator->GetWord(word)) {
        if (word.nFontIndex != nCurFontIndex) {
          if (sWords.GetSize() > 0) {
            sEditStream << GetWordRenderString(sWords.MakeString());
            sWords.Clear();
          }
          sEditStream << GetFontSetString(pEdit->GetFontMap(), word.nFontIndex,
                                          word.fFontSize);
          nCurFontIndex = word.nFontIndex;
        }

        sWords << GetPDFWordString(pEdit->GetFontMap(), nCurFontIndex,
                                   word.Word, SubWord);
      }

      oldplace = place;
    } else {
      CPVT_Word word;
      if (pIterator->GetWord(word)) {
        ptNew = CFX_FloatPoint(word.ptWord.x + ptOffset.x,
                               word.ptWord.y + ptOffset.y);

        if (ptNew.x != ptOld.x || ptNew.y != ptOld.y) {
          sEditStream << ptNew.x - ptOld.x << " " << ptNew.y - ptOld.y
                      << " Td\n";
          ptOld = ptNew;
        }

        if (word.nFontIndex != nCurFontIndex) {
          sEditStream << GetFontSetString(pEdit->GetFontMap(), word.nFontIndex,
                                          word.fFontSize);
          nCurFontIndex = word.nFontIndex;
        }

        sEditStream << GetWordRenderString(GetPDFWordString(
            pEdit->GetFontMap(), nCurFontIndex, word.Word, SubWord));
      }
    }
  }

  if (sWords.GetSize() > 0) {
    sEditStream << GetWordRenderString(sWords.MakeString());
    sWords.Clear();
  }

  CFX_ByteTextBuf sAppStream;
  if (sEditStream.GetSize() > 0) {
    int32_t nHorzScale = pEdit->GetHorzScale();
    if (nHorzScale != 100) {
      sAppStream << nHorzScale << " Tz\n";
    }

    FX_FLOAT fCharSpace = pEdit->GetCharSpace();
    if (!FX_EDIT_IsFloatZero(fCharSpace)) {
      sAppStream << fCharSpace << " Tc\n";
    }

    sAppStream << sEditStream;
  }

  return sAppStream.MakeString();
}

CFX_ByteString IFX_Edit::GetSelectAppearanceStream(
    IFX_Edit* pEdit,
    const CFX_FloatPoint& ptOffset,
    const CPVT_WordRange* pRange) {
  if (!pRange || !pRange->IsExist())
    return CFX_ByteString();

  IFX_Edit_Iterator* pIterator = pEdit->GetIterator();
  pIterator->SetAt(pRange->BeginPos);

  CFX_ByteTextBuf sRet;
  while (pIterator->NextWord()) {
    CPVT_WordPlace place = pIterator->GetAt();
    if (place.WordCmp(pRange->EndPos) > 0)
      break;

    CPVT_Word word;
    CPVT_Line line;
    if (pIterator->GetWord(word) && pIterator->GetLine(line)) {
      sRet << word.ptWord.x + ptOffset.x << " "
           << line.ptLine.y + line.fLineDescent << " " << word.fWidth << " "
           << line.fLineAscent - line.fLineDescent << " re\nf\n";
    }
  }

  return sRet.MakeString();
}
