// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/edit/cpdf_pagecontentgenerator.h"

#include "core/fpdfapi/edit/cpdf_creator.h"
#include "core/fpdfapi/page/cpdf_docpagedata.h"
#include "core/fpdfapi/page/cpdf_image.h"
#include "core/fpdfapi/page/cpdf_imageobject.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fpdfapi/page/pageint.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"

CFX_ByteTextBuf& operator<<(CFX_ByteTextBuf& ar, CFX_Matrix& matrix) {
  ar << matrix.a << " " << matrix.b << " " << matrix.c << " " << matrix.d << " "
     << matrix.e << " " << matrix.f;
  return ar;
}

CPDF_PageContentGenerator::CPDF_PageContentGenerator(CPDF_Page* pPage)
    : m_pPage(pPage), m_pDocument(m_pPage->m_pDocument) {
  for (const auto& pObj : *pPage->GetPageObjectList())
    InsertPageObject(pObj.get());
}

CPDF_PageContentGenerator::~CPDF_PageContentGenerator() {}

void CPDF_PageContentGenerator::InsertPageObject(CPDF_PageObject* pPageObject) {
  if (pPageObject)
    m_pageObjects.push_back(pPageObject);
}

void CPDF_PageContentGenerator::GenerateContent() {
  CFX_ByteTextBuf buf;
  for (CPDF_PageObject* pPageObj : m_pageObjects) {
    CPDF_ImageObject* pImageObject = pPageObj->AsImage();
    if (pImageObject)
      ProcessImage(buf, pImageObject);
  }
  CPDF_Dictionary* pPageDict = m_pPage->m_pFormDict;
  CPDF_Object* pContent =
      pPageDict ? pPageDict->GetDirectObjectFor("Contents") : nullptr;
  if (pContent)
    pPageDict->RemoveFor("Contents");

  CPDF_Stream* pStream = m_pDocument->NewIndirect<CPDF_Stream>();
  pStream->SetData(buf.GetBuffer(), buf.GetLength());
  pPageDict->SetReferenceFor("Contents", m_pDocument, pStream);
}

CFX_ByteString CPDF_PageContentGenerator::RealizeResource(
    uint32_t dwResourceObjNum,
    const CFX_ByteString& bsType) {
  ASSERT(dwResourceObjNum);
  if (!m_pPage->m_pResources) {
    m_pPage->m_pResources = m_pDocument->NewIndirect<CPDF_Dictionary>();
    m_pPage->m_pFormDict->SetReferenceFor("Resources", m_pDocument,
                                          m_pPage->m_pResources);
  }
  CPDF_Dictionary* pResList = m_pPage->m_pResources->GetDictFor(bsType);
  if (!pResList) {
    pResList = new CPDF_Dictionary(m_pDocument->GetByteStringPool());
    m_pPage->m_pResources->SetFor(bsType, pResList);
  }
  CFX_ByteString name;
  int idnum = 1;
  while (1) {
    name.Format("FX%c%d", bsType[0], idnum);
    if (!pResList->KeyExist(name)) {
      break;
    }
    idnum++;
  }
  pResList->SetReferenceFor(name, m_pDocument, dwResourceObjNum);
  return name;
}

void CPDF_PageContentGenerator::ProcessImage(CFX_ByteTextBuf& buf,
                                             CPDF_ImageObject* pImageObj) {
  if ((pImageObj->m_Matrix.a == 0 && pImageObj->m_Matrix.b == 0) ||
      (pImageObj->m_Matrix.c == 0 && pImageObj->m_Matrix.d == 0)) {
    return;
  }
  buf << "q " << pImageObj->m_Matrix << " cm ";

  CPDF_Image* pImage = pImageObj->GetImage();
  if (pImage->IsInline())
    return;

  CPDF_Stream* pStream = pImage->GetStream();
  if (!pStream)
    return;

  bool bWasInline = pStream->IsInline();
  if (bWasInline)
    pImage->ConvertStreamToIndirectObject();

  uint32_t dwObjNum = pStream->GetObjNum();
  CFX_ByteString name = RealizeResource(dwObjNum, "XObject");
  if (bWasInline)
    pImageObj->SetUnownedImage(m_pDocument->GetPageData()->GetImage(dwObjNum));

  buf << "/" << PDF_NameEncode(name) << " Do Q\n";
}

void CPDF_PageContentGenerator::ProcessForm(CFX_ByteTextBuf& buf,
                                            const uint8_t* data,
                                            uint32_t size,
                                            CFX_Matrix& matrix) {
  if (!data || !size)
    return;

  buf << "q " << matrix << " cm ";

  CFX_FloatRect bbox = m_pPage->GetPageBBox();
  matrix.TransformRect(bbox);

  CPDF_Dictionary* pFormDict =
      new CPDF_Dictionary(m_pDocument->GetByteStringPool());
  pFormDict->SetNameFor("Type", "XObject");
  pFormDict->SetNameFor("Subtype", "Form");
  pFormDict->SetRectFor("BBox", bbox);

  CPDF_Stream* pStream = m_pDocument->NewIndirect<CPDF_Stream>();
  pStream->InitStream(data, size, pFormDict);

  CFX_ByteString name = RealizeResource(pStream->GetObjNum(), "XObject");
  buf << "/" << PDF_NameEncode(name) << " Do Q\n";
}

void CPDF_PageContentGenerator::TransformContent(CFX_Matrix& matrix) {
  CPDF_Dictionary* pDict = m_pPage->m_pFormDict;
  CPDF_Object* pContent =
      pDict ? pDict->GetDirectObjectFor("Contents") : nullptr;
  if (!pContent)
    return;

  CFX_ByteTextBuf buf;
  if (CPDF_Array* pArray = pContent->AsArray()) {
    size_t iCount = pArray->GetCount();
    CPDF_StreamAcc** pContentArray = FX_Alloc(CPDF_StreamAcc*, iCount);
    size_t size = 0;
    for (size_t i = 0; i < iCount; ++i) {
      pContent = pArray->GetObjectAt(i);
      CPDF_Stream* pStream = ToStream(pContent);
      if (!pStream)
        continue;

      CPDF_StreamAcc* pStreamAcc = new CPDF_StreamAcc();
      pStreamAcc->LoadAllData(pStream);
      pContentArray[i] = pStreamAcc;
      size += pContentArray[i]->GetSize() + 1;
    }
    int pos = 0;
    uint8_t* pBuf = FX_Alloc(uint8_t, size);
    for (size_t i = 0; i < iCount; ++i) {
      FXSYS_memcpy(pBuf + pos, pContentArray[i]->GetData(),
                   pContentArray[i]->GetSize());
      pos += pContentArray[i]->GetSize() + 1;
      pBuf[pos - 1] = ' ';
      delete pContentArray[i];
    }
    ProcessForm(buf, pBuf, size, matrix);
    FX_Free(pBuf);
    FX_Free(pContentArray);
  } else if (CPDF_Stream* pStream = pContent->AsStream()) {
    CPDF_StreamAcc contentStream;
    contentStream.LoadAllData(pStream);
    ProcessForm(buf, contentStream.GetData(), contentStream.GetSize(), matrix);
  }
  CPDF_Stream* pStream = m_pDocument->NewIndirect<CPDF_Stream>();
  pStream->SetData(buf.GetBuffer(), buf.GetLength());
  m_pPage->m_pFormDict->SetReferenceFor("Contents", m_pDocument, pStream);
}
