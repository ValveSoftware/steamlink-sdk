// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PARSER_CFDF_DOCUMENT_H_
#define CORE_FPDFAPI_PARSER_CFDF_DOCUMENT_H_

#include "core/fpdfapi/parser/cpdf_indirect_object_holder.h"
#include "core/fpdfapi/parser/cpdf_object.h"
#include "core/fxcrt/fx_basic.h"

class CPDF_Dictionary;

class CFDF_Document : public CPDF_IndirectObjectHolder {
 public:
  static CFDF_Document* CreateNewDoc();
  static CFDF_Document* ParseFile(IFX_SeekableReadStream* pFile,
                                  bool bOwnFile = false);
  static CFDF_Document* ParseMemory(const uint8_t* pData, uint32_t size);
  ~CFDF_Document() override;

  bool WriteBuf(CFX_ByteTextBuf& buf) const;
  CPDF_Dictionary* GetRoot() const { return m_pRootDict; }

 protected:
  CFDF_Document();
  void ParseStream(IFX_SeekableReadStream* pFile, bool bOwnFile);

  CPDF_Dictionary* m_pRootDict;
  IFX_SeekableReadStream* m_pFile;
  bool m_bOwnFile;
};

#endif  // CORE_FPDFAPI_PARSER_CFDF_DOCUMENT_H_
