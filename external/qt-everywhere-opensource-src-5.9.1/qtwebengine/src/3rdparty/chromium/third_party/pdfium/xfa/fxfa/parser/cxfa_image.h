// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_CXFA_IMAGE_H_
#define XFA_FXFA_PARSER_CXFA_IMAGE_H_

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "xfa/fxfa/parser/cxfa_data.h"

class CXFA_Node;

class CXFA_Image : public CXFA_Data {
 public:
  CXFA_Image(CXFA_Node* pNode, bool bDefValue);

  int32_t GetAspect();
  bool GetContentType(CFX_WideString& wsContentType);
  bool GetHref(CFX_WideString& wsHref);
  int32_t GetTransferEncoding();
  bool GetContent(CFX_WideString& wsText);
  bool SetContentType(const CFX_WideString& wsContentType);
  bool SetHref(const CFX_WideString& wsHref);
  bool SetTransferEncoding(int32_t iTransferEncoding);

 protected:
  bool m_bDefValue;
};

#endif  // XFA_FXFA_PARSER_CXFA_IMAGE_H_
