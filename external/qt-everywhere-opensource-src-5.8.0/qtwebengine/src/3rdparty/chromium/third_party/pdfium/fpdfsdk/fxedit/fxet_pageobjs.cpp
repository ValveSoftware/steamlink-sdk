// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/fpdf_page/include/cpdf_pageobject.h"
#include "core/fpdfapi/fpdf_page/include/cpdf_pageobjectholder.h"
#include "core/fpdfapi/fpdf_page/include/cpdf_pathobject.h"
#include "core/fpdfapi/fpdf_page/include/cpdf_textobject.h"
#include "core/fpdfapi/fpdf_render/include/cpdf_renderoptions.h"
#include "core/fpdfapi/fpdf_render/include/cpdf_textrenderer.h"
#include "core/fpdfdoc/include/cpvt_word.h"
#include "core/fpdfdoc/include/ipvt_fontmap.h"
#include "core/fxge/include/fx_ge.h"
#include "fpdfsdk/cfx_systemhandler.h"
#include "fpdfsdk/fxedit/include/fx_edit.h"
#include "fpdfsdk/fxedit/include/fxet_edit.h"

namespace {

CFX_FloatRect GetUnderLineRect(const CPVT_Word& word) {
  return CFX_FloatRect(word.ptWord.x, word.ptWord.y + word.fDescent * 0.5f,
                       word.ptWord.x + word.fWidth,
                       word.ptWord.y + word.fDescent * 0.25f);
}

CFX_FloatRect GetCrossoutRect(const CPVT_Word& word) {
  return CFX_FloatRect(word.ptWord.x,
                       word.ptWord.y + (word.fAscent + word.fDescent) * 0.5f +
                           word.fDescent * 0.25f,
                       word.ptWord.x + word.fWidth,
                       word.ptWord.y + (word.fAscent + word.fDescent) * 0.5f);
}

void DrawTextString(CFX_RenderDevice* pDevice,
                    const CFX_FloatPoint& pt,
                    CPDF_Font* pFont,
                    FX_FLOAT fFontSize,
                    CFX_Matrix* pUser2Device,
                    const CFX_ByteString& str,
                    FX_ARGB crTextFill,
                    FX_ARGB crTextStroke,
                    int32_t nHorzScale) {
  FX_FLOAT x = pt.x, y = pt.y;
  pUser2Device->Transform(x, y);

  if (pFont) {
    if (nHorzScale != 100) {
      CFX_Matrix mt(nHorzScale / 100.0f, 0, 0, 1, 0, 0);
      mt.Concat(*pUser2Device);

      CPDF_RenderOptions ro;
      ro.m_Flags = RENDER_CLEARTYPE;
      ro.m_ColorMode = RENDER_COLOR_NORMAL;

      if (crTextStroke != 0) {
        CFX_FloatPoint pt1(0, 0), pt2(1, 0);
        pUser2Device->Transform(pt1.x, pt1.y);
        pUser2Device->Transform(pt2.x, pt2.y);
        CFX_GraphStateData gsd;
        gsd.m_LineWidth =
            (FX_FLOAT)FXSYS_fabs((pt2.x + pt2.y) - (pt1.x + pt1.y));

        CPDF_TextRenderer::DrawTextString(pDevice, x, y, pFont, fFontSize, &mt,
                                          str, crTextFill, crTextStroke, &gsd,
                                          &ro);
      } else {
        CPDF_TextRenderer::DrawTextString(pDevice, x, y, pFont, fFontSize, &mt,
                                          str, crTextFill, 0, nullptr, &ro);
      }
    } else {
      CPDF_RenderOptions ro;
      ro.m_Flags = RENDER_CLEARTYPE;
      ro.m_ColorMode = RENDER_COLOR_NORMAL;

      if (crTextStroke != 0) {
        CFX_FloatPoint pt1(0, 0), pt2(1, 0);
        pUser2Device->Transform(pt1.x, pt1.y);
        pUser2Device->Transform(pt2.x, pt2.y);
        CFX_GraphStateData gsd;
        gsd.m_LineWidth =
            (FX_FLOAT)FXSYS_fabs((pt2.x + pt2.y) - (pt1.x + pt1.y));

        CPDF_TextRenderer::DrawTextString(pDevice, x, y, pFont, fFontSize,
                                          pUser2Device, str, crTextFill,
                                          crTextStroke, &gsd, &ro);
      } else {
        CPDF_TextRenderer::DrawTextString(pDevice, x, y, pFont, fFontSize,
                                          pUser2Device, str, crTextFill, 0,
                                          nullptr, &ro);
      }
    }
  }
}

void AddRectToPageObjects(CPDF_PageObjectHolder* pObjectHolder,
                          FX_COLORREF crFill,
                          const CFX_FloatRect& rcFill) {
  std::unique_ptr<CPDF_PathObject> pPathObj(new CPDF_PathObject);
  CFX_PathData* pPathData = pPathObj->m_Path.GetModify();
  pPathData->AppendRect(rcFill.left, rcFill.bottom, rcFill.right, rcFill.top);

  FX_FLOAT rgb[3];
  rgb[0] = FXARGB_R(crFill) / 255.0f;
  rgb[1] = FXARGB_G(crFill) / 255.0f;
  rgb[2] = FXARGB_B(crFill) / 255.0f;
  pPathObj->m_ColorState.SetFillColor(
      CPDF_ColorSpace::GetStockCS(PDFCS_DEVICERGB), rgb, 3);

  pPathObj->m_FillType = FXFILL_ALTERNATE;
  pPathObj->m_bStroke = FALSE;
  pObjectHolder->GetPageObjectList()->push_back(std::move(pPathObj));
}

CPDF_TextObject* AddTextObjToPageObjects(CPDF_PageObjectHolder* pObjectHolder,
                                         FX_COLORREF crText,
                                         CPDF_Font* pFont,
                                         FX_FLOAT fFontSize,
                                         FX_FLOAT fCharSpace,
                                         int32_t nHorzScale,
                                         const CFX_FloatPoint& point,
                                         const CFX_ByteString& text) {
  std::unique_ptr<CPDF_TextObject> pTxtObj(new CPDF_TextObject);
  CPDF_TextStateData* pTextStateData = pTxtObj->m_TextState.GetModify();
  pTextStateData->m_pFont = pFont;
  pTextStateData->m_FontSize = fFontSize;
  pTextStateData->m_CharSpace = fCharSpace;
  pTextStateData->m_WordSpace = 0;
  pTextStateData->m_TextMode = TextRenderingMode::MODE_FILL;
  pTextStateData->m_Matrix[0] = nHorzScale / 100.0f;
  pTextStateData->m_Matrix[1] = 0;
  pTextStateData->m_Matrix[2] = 0;
  pTextStateData->m_Matrix[3] = 1;

  FX_FLOAT rgb[3];
  rgb[0] = FXARGB_R(crText) / 255.0f;
  rgb[1] = FXARGB_G(crText) / 255.0f;
  rgb[2] = FXARGB_B(crText) / 255.0f;
  pTxtObj->m_ColorState.SetFillColor(
      CPDF_ColorSpace::GetStockCS(PDFCS_DEVICERGB), rgb, 3);
  pTxtObj->m_ColorState.SetStrokeColor(
      CPDF_ColorSpace::GetStockCS(PDFCS_DEVICERGB), rgb, 3);

  pTxtObj->SetPosition(point.x, point.y);
  pTxtObj->SetText(text);

  CPDF_TextObject* pRet = pTxtObj.get();
  pObjectHolder->GetPageObjectList()->push_back(std::move(pTxtObj));
  return pRet;
}

}  // namespace

void IFX_Edit::DrawUnderline(CFX_RenderDevice* pDevice,
                             CFX_Matrix* pUser2Device,
                             IFX_Edit* pEdit,
                             FX_COLORREF color,
                             const CFX_FloatRect& rcClip,
                             const CFX_FloatPoint& ptOffset,
                             const CPVT_WordRange* pRange) {
  pDevice->SaveState();

  if (!rcClip.IsEmpty()) {
    CFX_FloatRect rcTemp = rcClip;
    pUser2Device->TransformRect(rcTemp);
    pDevice->SetClip_Rect(rcTemp.ToFxRect());
  }

  IFX_Edit_Iterator* pIterator = pEdit->GetIterator();
  if (pEdit->GetFontMap()) {
    if (pRange)
      pIterator->SetAt(pRange->BeginPos);
    else
      pIterator->SetAt(0);

    while (pIterator->NextWord()) {
      CPVT_WordPlace place = pIterator->GetAt();
      if (pRange && place.WordCmp(pRange->EndPos) > 0)
        break;

      CPVT_Word word;
      if (pIterator->GetWord(word)) {
        CFX_PathData pathUnderline;
        CFX_FloatRect rcUnderline = GetUnderLineRect(word);
        rcUnderline.left += ptOffset.x;
        rcUnderline.right += ptOffset.x;
        rcUnderline.top += ptOffset.y;
        rcUnderline.bottom += ptOffset.y;
        pathUnderline.AppendRect(rcUnderline.left, rcUnderline.bottom,
                                 rcUnderline.right, rcUnderline.top);

        pDevice->DrawPath(&pathUnderline, pUser2Device, nullptr, color, 0,
                          FXFILL_WINDING);
      }
    }
  }

  pDevice->RestoreState(false);
}

void IFX_Edit::DrawEdit(CFX_RenderDevice* pDevice,
                        CFX_Matrix* pUser2Device,
                        IFX_Edit* pEdit,
                        FX_COLORREF crTextFill,
                        FX_COLORREF crTextStroke,
                        const CFX_FloatRect& rcClip,
                        const CFX_FloatPoint& ptOffset,
                        const CPVT_WordRange* pRange,
                        CFX_SystemHandler* pSystemHandler,
                        void* pFFLData) {
  const bool bContinuous =
      pEdit->GetCharArray() == 0 && pEdit->GetCharSpace() <= 0.0f;
  uint16_t SubWord = pEdit->GetPasswordChar();
  FX_FLOAT fFontSize = pEdit->GetFontSize();
  CPVT_WordRange wrSelect = pEdit->GetSelectWordRange();
  int32_t nHorzScale = pEdit->GetHorzScale();

  FX_COLORREF crCurFill = crTextFill;
  FX_COLORREF crOldFill = crCurFill;

  FX_BOOL bSelect = FALSE;
  const FX_COLORREF crWhite = ArgbEncode(255, 255, 255, 255);
  const FX_COLORREF crSelBK = ArgbEncode(255, 0, 51, 113);

  CFX_ByteTextBuf sTextBuf;
  int32_t nFontIndex = -1;
  CFX_FloatPoint ptBT(0.0f, 0.0f);

  pDevice->SaveState();

  if (!rcClip.IsEmpty()) {
    CFX_FloatRect rcTemp = rcClip;
    pUser2Device->TransformRect(rcTemp);
    pDevice->SetClip_Rect(rcTemp.ToFxRect());
  }

  IFX_Edit_Iterator* pIterator = pEdit->GetIterator();
  if (IPVT_FontMap* pFontMap = pEdit->GetFontMap()) {
    if (pRange)
      pIterator->SetAt(pRange->BeginPos);
    else
      pIterator->SetAt(0);

    CPVT_WordPlace oldplace;
    while (pIterator->NextWord()) {
      CPVT_WordPlace place = pIterator->GetAt();
      if (pRange && place.WordCmp(pRange->EndPos) > 0)
        break;

      if (wrSelect.IsExist()) {
        bSelect = place.WordCmp(wrSelect.BeginPos) > 0 &&
                  place.WordCmp(wrSelect.EndPos) <= 0;
        crCurFill = bSelect ? crWhite : crTextFill;
      }
      if (pSystemHandler && pSystemHandler->IsSelectionImplemented()) {
        crCurFill = crTextFill;
        crOldFill = crCurFill;
      }
      CPVT_Word word;
      if (pIterator->GetWord(word)) {
        if (bSelect) {
          CPVT_Line line;
          pIterator->GetLine(line);

          if (pSystemHandler && pSystemHandler->IsSelectionImplemented()) {
            CFX_FloatRect rc(word.ptWord.x, line.ptLine.y + line.fLineDescent,
                             word.ptWord.x + word.fWidth,
                             line.ptLine.y + line.fLineAscent);
            rc.Intersect(rcClip);
            pSystemHandler->OutputSelectedRect(pFFLData, rc);
          } else {
            CFX_PathData pathSelBK;
            pathSelBK.AppendRect(
                word.ptWord.x, line.ptLine.y + line.fLineDescent,
                word.ptWord.x + word.fWidth, line.ptLine.y + line.fLineAscent);

            pDevice->DrawPath(&pathSelBK, pUser2Device, nullptr, crSelBK, 0,
                              FXFILL_WINDING);
          }
        }

        if (bContinuous) {
          if (place.LineCmp(oldplace) != 0 || word.nFontIndex != nFontIndex ||
              crOldFill != crCurFill) {
            if (sTextBuf.GetLength() > 0) {
              DrawTextString(
                  pDevice,
                  CFX_FloatPoint(ptBT.x + ptOffset.x, ptBT.y + ptOffset.y),
                  pFontMap->GetPDFFont(nFontIndex), fFontSize, pUser2Device,
                  sTextBuf.MakeString(), crOldFill, crTextStroke, nHorzScale);

              sTextBuf.Clear();
            }
            nFontIndex = word.nFontIndex;
            ptBT = word.ptWord;
            crOldFill = crCurFill;
          }

          sTextBuf << GetPDFWordString(pFontMap, word.nFontIndex, word.Word,
                                       SubWord)
                          .AsStringC();
        } else {
          DrawTextString(
              pDevice, CFX_FloatPoint(word.ptWord.x + ptOffset.x,
                                      word.ptWord.y + ptOffset.y),
              pFontMap->GetPDFFont(word.nFontIndex), fFontSize, pUser2Device,
              GetPDFWordString(pFontMap, word.nFontIndex, word.Word, SubWord),
              crCurFill, crTextStroke, nHorzScale);
        }
        oldplace = place;
      }
    }

    if (sTextBuf.GetLength() > 0) {
      DrawTextString(
          pDevice, CFX_FloatPoint(ptBT.x + ptOffset.x, ptBT.y + ptOffset.y),
          pFontMap->GetPDFFont(nFontIndex), fFontSize, pUser2Device,
          sTextBuf.MakeString(), crOldFill, crTextStroke, nHorzScale);
    }
  }

  pDevice->RestoreState(false);
}

void IFX_Edit::DrawRichEdit(CFX_RenderDevice* pDevice,
                            CFX_Matrix* pUser2Device,
                            IFX_Edit* pEdit,
                            const CFX_FloatRect& rcClip,
                            const CFX_FloatPoint& ptOffset,
                            const CPVT_WordRange* pRange) {
  CPVT_WordRange wrSelect = pEdit->GetSelectWordRange();

  FX_COLORREF crCurText = ArgbEncode(255, 0, 0, 0);
  FX_COLORREF crOld = crCurText;
  FX_BOOL bSelect = FALSE;
  const FX_COLORREF crWhite = ArgbEncode(255, 255, 255, 255);
  const FX_COLORREF crSelBK = ArgbEncode(255, 0, 51, 113);

  CFX_ByteTextBuf sTextBuf;
  CPVT_WordProps wp;
  CFX_FloatPoint ptBT(0.0f, 0.0f);

  pDevice->SaveState();

  if (!rcClip.IsEmpty()) {
    CFX_FloatRect rcTemp = rcClip;
    pUser2Device->TransformRect(rcTemp);
    pDevice->SetClip_Rect(rcTemp.ToFxRect());
  }

  IFX_Edit_Iterator* pIterator = pEdit->GetIterator();
  if (IPVT_FontMap* pFontMap = pEdit->GetFontMap()) {
    if (pRange)
      pIterator->SetAt(pRange->BeginPos);
    else
      pIterator->SetAt(0);

    CPVT_WordPlace oldplace;

    while (pIterator->NextWord()) {
      CPVT_WordPlace place = pIterator->GetAt();
      if (pRange && place.WordCmp(pRange->EndPos) > 0)
        break;

      CPVT_Word word;
      if (pIterator->GetWord(word)) {
        word.WordProps.fFontSize = word.fFontSize;

        crCurText = ArgbEncode(255, word.WordProps.dwWordColor);

        if (wrSelect.IsExist()) {
          bSelect = place.WordCmp(wrSelect.BeginPos) > 0 &&
                    place.WordCmp(wrSelect.EndPos) <= 0;
          if (bSelect) {
            crCurText = crWhite;
          }
        }

        if (bSelect) {
          CPVT_Line line;
          pIterator->GetLine(line);

          CFX_PathData pathSelBK;
          pathSelBK.AppendRect(word.ptWord.x + ptOffset.x,
                               line.ptLine.y + line.fLineDescent + ptOffset.y,
                               word.ptWord.x + word.fWidth + ptOffset.x,
                               line.ptLine.y + line.fLineAscent + ptOffset.y);

          pDevice->DrawPath(&pathSelBK, pUser2Device, nullptr, crSelBK, 0,
                            FXFILL_WINDING);
        }

        if (place.LineCmp(oldplace) != 0 || word.WordProps.fCharSpace > 0.0f ||
            word.WordProps.nHorzScale != 100 ||
            FXSYS_memcmp(&word.WordProps, &wp, sizeof(CPVT_WordProps)) != 0 ||
            crOld != crCurText) {
          if (sTextBuf.GetLength() > 0) {
            DrawTextString(
                pDevice,
                CFX_FloatPoint(ptBT.x + ptOffset.x, ptBT.y + ptOffset.y),
                pFontMap->GetPDFFont(wp.nFontIndex), wp.fFontSize, pUser2Device,
                sTextBuf.MakeString(), crOld, 0, wp.nHorzScale);

            sTextBuf.Clear();
          }
          wp = word.WordProps;
          ptBT = word.ptWord;
          crOld = crCurText;
        }

        sTextBuf << GetPDFWordString(pFontMap, word.WordProps.nFontIndex,
                                     word.Word, 0)
                        .AsStringC();

        if (word.WordProps.nWordStyle & PVTWORD_STYLE_UNDERLINE) {
          CFX_PathData pathUnderline;
          CFX_FloatRect rcUnderline = GetUnderLineRect(word);
          pathUnderline.AppendRect(rcUnderline.left, rcUnderline.bottom,
                                   rcUnderline.right, rcUnderline.top);

          pDevice->DrawPath(&pathUnderline, pUser2Device, nullptr, crCurText, 0,
                            FXFILL_WINDING);
        }

        if (word.WordProps.nWordStyle & PVTWORD_STYLE_CROSSOUT) {
          CFX_PathData pathCrossout;
          CFX_FloatRect rcCrossout = GetCrossoutRect(word);
          pathCrossout.AppendRect(rcCrossout.left, rcCrossout.bottom,
                                  rcCrossout.right, rcCrossout.top);

          pDevice->DrawPath(&pathCrossout, pUser2Device, nullptr, crCurText, 0,
                            FXFILL_WINDING);
        }

        oldplace = place;
      }
    }

    if (sTextBuf.GetLength() > 0) {
      DrawTextString(
          pDevice, CFX_FloatPoint(ptBT.x + ptOffset.x, ptBT.y + ptOffset.y),
          pFontMap->GetPDFFont(wp.nFontIndex), wp.fFontSize, pUser2Device,
          sTextBuf.MakeString(), crOld, 0, wp.nHorzScale);
    }
  }

  pDevice->RestoreState(false);
}

void IFX_Edit::GeneratePageObjects(
    CPDF_PageObjectHolder* pObjectHolder,
    IFX_Edit* pEdit,
    const CFX_FloatPoint& ptOffset,
    const CPVT_WordRange* pRange,
    FX_COLORREF crText,
    CFX_ArrayTemplate<CPDF_TextObject*>& ObjArray) {
  FX_FLOAT fFontSize = pEdit->GetFontSize();

  int32_t nOldFontIndex = -1;

  CFX_ByteTextBuf sTextBuf;
  CFX_FloatPoint ptBT(0.0f, 0.0f);

  ObjArray.RemoveAll();

  IFX_Edit_Iterator* pIterator = pEdit->GetIterator();
  if (IPVT_FontMap* pFontMap = pEdit->GetFontMap()) {
    if (pRange)
      pIterator->SetAt(pRange->BeginPos);
    else
      pIterator->SetAt(0);

    CPVT_WordPlace oldplace;

    while (pIterator->NextWord()) {
      CPVT_WordPlace place = pIterator->GetAt();
      if (pRange && place.WordCmp(pRange->EndPos) > 0)
        break;

      CPVT_Word word;
      if (pIterator->GetWord(word)) {
        if (place.LineCmp(oldplace) != 0 || nOldFontIndex != word.nFontIndex) {
          if (sTextBuf.GetLength() > 0) {
            ObjArray.Add(AddTextObjToPageObjects(
                pObjectHolder, crText, pFontMap->GetPDFFont(nOldFontIndex),
                fFontSize, 0.0f, 100,
                CFX_FloatPoint(ptBT.x + ptOffset.x, ptBT.y + ptOffset.y),
                sTextBuf.MakeString()));

            sTextBuf.Clear();
          }

          ptBT = word.ptWord;
          nOldFontIndex = word.nFontIndex;
        }

        sTextBuf << GetPDFWordString(pFontMap, word.nFontIndex, word.Word, 0)
                        .AsStringC();
        oldplace = place;
      }
    }

    if (sTextBuf.GetLength() > 0) {
      ObjArray.Add(AddTextObjToPageObjects(
          pObjectHolder, crText, pFontMap->GetPDFFont(nOldFontIndex), fFontSize,
          0.0f, 100, CFX_FloatPoint(ptBT.x + ptOffset.x, ptBT.y + ptOffset.y),
          sTextBuf.MakeString()));
    }
  }
}

void IFX_Edit::GenerateRichPageObjects(
    CPDF_PageObjectHolder* pObjectHolder,
    IFX_Edit* pEdit,
    const CFX_FloatPoint& ptOffset,
    const CPVT_WordRange* pRange,
    CFX_ArrayTemplate<CPDF_TextObject*>& ObjArray) {
  FX_COLORREF crCurText = ArgbEncode(255, 0, 0, 0);
  FX_COLORREF crOld = crCurText;

  CFX_ByteTextBuf sTextBuf;
  CPVT_WordProps wp;
  CFX_FloatPoint ptBT(0.0f, 0.0f);

  ObjArray.RemoveAll();

  IFX_Edit_Iterator* pIterator = pEdit->GetIterator();
  if (IPVT_FontMap* pFontMap = pEdit->GetFontMap()) {
    if (pRange)
      pIterator->SetAt(pRange->BeginPos);
    else
      pIterator->SetAt(0);

    CPVT_WordPlace oldplace;

    while (pIterator->NextWord()) {
      CPVT_WordPlace place = pIterator->GetAt();
      if (pRange && place.WordCmp(pRange->EndPos) > 0)
        break;

      CPVT_Word word;
      if (pIterator->GetWord(word)) {
        word.WordProps.fFontSize = word.fFontSize;

        crCurText = ArgbEncode(255, word.WordProps.dwWordColor);

        if (place.LineCmp(oldplace) != 0 || word.WordProps.fCharSpace > 0.0f ||
            word.WordProps.nHorzScale != 100 ||
            FXSYS_memcmp(&word.WordProps, &wp, sizeof(CPVT_WordProps)) != 0 ||
            crOld != crCurText) {
          if (sTextBuf.GetLength() > 0) {
            ObjArray.Add(AddTextObjToPageObjects(
                pObjectHolder, crOld, pFontMap->GetPDFFont(wp.nFontIndex),
                wp.fFontSize, wp.fCharSpace, wp.nHorzScale,
                CFX_FloatPoint(ptBT.x + ptOffset.x, ptBT.y + ptOffset.y),
                sTextBuf.MakeString()));

            sTextBuf.Clear();
          }

          wp = word.WordProps;
          ptBT = word.ptWord;
          crOld = crCurText;
        }

        sTextBuf << GetPDFWordString(pFontMap, word.WordProps.nFontIndex,
                                     word.Word, 0)
                        .AsStringC();

        if (word.WordProps.nWordStyle & PVTWORD_STYLE_UNDERLINE) {
          CFX_FloatRect rcUnderline = GetUnderLineRect(word);
          rcUnderline.left += ptOffset.x;
          rcUnderline.right += ptOffset.x;
          rcUnderline.top += ptOffset.y;
          rcUnderline.bottom += ptOffset.y;

          AddRectToPageObjects(pObjectHolder, crCurText, rcUnderline);
        }

        if (word.WordProps.nWordStyle & PVTWORD_STYLE_CROSSOUT) {
          CFX_FloatRect rcCrossout = GetCrossoutRect(word);
          rcCrossout.left += ptOffset.x;
          rcCrossout.right += ptOffset.x;
          rcCrossout.top += ptOffset.y;
          rcCrossout.bottom += ptOffset.y;

          AddRectToPageObjects(pObjectHolder, crCurText, rcCrossout);
        }

        oldplace = place;
      }
    }

    if (sTextBuf.GetLength() > 0) {
      ObjArray.Add(AddTextObjToPageObjects(
          pObjectHolder, crOld, pFontMap->GetPDFFont(wp.nFontIndex),
          wp.fFontSize, wp.fCharSpace, wp.nHorzScale,
          CFX_FloatPoint(ptBT.x + ptOffset.x, ptBT.y + ptOffset.y),
          sTextBuf.MakeString()));
    }
  }
}

void IFX_Edit::GenerateUnderlineObjects(CPDF_PageObjectHolder* pObjectHolder,
                                        IFX_Edit* pEdit,
                                        const CFX_FloatPoint& ptOffset,
                                        const CPVT_WordRange* pRange,
                                        FX_COLORREF color) {
  IFX_Edit_Iterator* pIterator = pEdit->GetIterator();
  if (pEdit->GetFontMap()) {
    if (pRange)
      pIterator->SetAt(pRange->BeginPos);
    else
      pIterator->SetAt(0);

    CPVT_WordPlace oldplace;

    while (pIterator->NextWord()) {
      CPVT_WordPlace place = pIterator->GetAt();
      if (pRange && place.WordCmp(pRange->EndPos) > 0)
        break;

      CPVT_Word word;
      if (pIterator->GetWord(word)) {
        CFX_FloatRect rcUnderline = GetUnderLineRect(word);
        rcUnderline.left += ptOffset.x;
        rcUnderline.right += ptOffset.x;
        rcUnderline.top += ptOffset.y;
        rcUnderline.bottom += ptOffset.y;
        AddRectToPageObjects(pObjectHolder, color, rcUnderline);
      }
    }
  }
}
