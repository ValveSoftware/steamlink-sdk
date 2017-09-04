// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_EDIT_CPDF_PAGECONTENTGENERATOR_H_
#define CORE_FPDFAPI_EDIT_CPDF_PAGECONTENTGENERATOR_H_

#include <vector>

#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fxcrt/fx_basic.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_system.h"

class CPDF_Object;
class CPDF_Page;
class CPDF_PageObject;
class CPDF_ImageObject;

class CPDF_PageContentGenerator {
 public:
  explicit CPDF_PageContentGenerator(CPDF_Page* pPage);
  ~CPDF_PageContentGenerator();

  void InsertPageObject(CPDF_PageObject* pPageObject);
  void GenerateContent();
  void TransformContent(CFX_Matrix& matrix);

 private:
  void ProcessImage(CFX_ByteTextBuf& buf, CPDF_ImageObject* pImageObj);
  void ProcessForm(CFX_ByteTextBuf& buf,
                   const uint8_t* data,
                   uint32_t size,
                   CFX_Matrix& matrix);
  CFX_ByteString RealizeResource(uint32_t dwResourceObjNum,
                                 const CFX_ByteString& bsType);

  CPDF_Page* m_pPage;
  CPDF_Document* m_pDocument;
  std::vector<CPDF_PageObject*> m_pageObjects;
};

#endif  // CORE_FPDFAPI_EDIT_CPDF_PAGECONTENTGENERATOR_H_
