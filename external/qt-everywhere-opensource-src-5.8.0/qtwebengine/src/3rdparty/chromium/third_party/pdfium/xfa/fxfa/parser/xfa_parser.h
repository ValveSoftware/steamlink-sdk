// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_XFA_PARSER_H_
#define XFA_FXFA_PARSER_XFA_PARSER_H_

#include "xfa/fxfa/parser/xfa_document.h"

class CFDE_XMLDoc;

class IXFA_Parser {
 public:
  static IXFA_Parser* Create(CXFA_Document* pFactory,
                             FX_BOOL bDocumentParser = FALSE);

  virtual ~IXFA_Parser() {}
  virtual void Release() = 0;
  virtual int32_t StartParse(IFX_FileRead* pStream,
                             XFA_XDPPACKET ePacketID = XFA_XDPPACKET_XDP) = 0;
  virtual int32_t DoParse(IFX_Pause* pPause = nullptr) = 0;
  virtual int32_t ParseXMLData(const CFX_WideString& wsXML,
                               CFDE_XMLNode*& pXMLNode,
                               IFX_Pause* pPause = nullptr) = 0;
  virtual void ConstructXFANode(CXFA_Node* pXFANode,
                                CFDE_XMLNode* pXMLNode) = 0;
  virtual CXFA_Document* GetFactory() const = 0;
  virtual CXFA_Node* GetRootNode() const = 0;
  virtual CFDE_XMLDoc* GetXMLDoc() const = 0;
  virtual void CloseParser() = 0;
};

#endif  // XFA_FXFA_PARSER_XFA_PARSER_H_
