// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_CXFA_DATAIMPORTER_H_
#define XFA_FXFA_PARSER_CXFA_DATAIMPORTER_H_

#include "core/fxcrt/fx_system.h"

class CXFA_Document;
class IFX_SeekableReadStream;

class CXFA_DataImporter {
 public:
  explicit CXFA_DataImporter(CXFA_Document* pDocument);

  bool ImportData(IFX_SeekableReadStream* pDataDocument);

 protected:
  CXFA_Document* const m_pDocument;
};

#endif  // XFA_FXFA_PARSER_CXFA_DATAIMPORTER_H_
