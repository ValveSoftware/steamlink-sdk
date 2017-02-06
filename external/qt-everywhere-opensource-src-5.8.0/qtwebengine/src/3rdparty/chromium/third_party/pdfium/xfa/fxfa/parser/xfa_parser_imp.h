// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_XFA_PARSER_IMP_H_
#define XFA_FXFA_PARSER_XFA_PARSER_IMP_H_

#include "xfa/fde/xml/fde_xml_imp.h"
#include "xfa/fxfa/parser/xfa_parser.h"

class CXFA_XMLParser;

class CXFA_SimpleParser : public IXFA_Parser {
 public:
  CXFA_SimpleParser(CXFA_Document* pFactory, FX_BOOL bDocumentParser = FALSE);
  ~CXFA_SimpleParser() override;

  // IXFA_Parser
  void Release() override;
  int32_t StartParse(IFX_FileRead* pStream,
                     XFA_XDPPACKET ePacketID = XFA_XDPPACKET_XDP) override;
  int32_t DoParse(IFX_Pause* pPause = nullptr) override;
  int32_t ParseXMLData(const CFX_WideString& wsXML,
                       CFDE_XMLNode*& pXMLNode,
                       IFX_Pause* pPause = nullptr) override;
  void ConstructXFANode(CXFA_Node* pXFANode, CFDE_XMLNode* pXMLNode) override;
  CXFA_Document* GetFactory() const override;
  CXFA_Node* GetRootNode() const override;
  CFDE_XMLDoc* GetXMLDoc() const override;
  void CloseParser() override;

 protected:
  CXFA_Node* ParseAsXDPPacket(CFDE_XMLNode* pXMLDocumentNode,
                              XFA_XDPPACKET ePacketID);
  CXFA_Node* ParseAsXDPPacket_XDP(CFDE_XMLNode* pXMLDocumentNode,
                                  XFA_XDPPACKET ePacketID);
  CXFA_Node* ParseAsXDPPacket_Config(CFDE_XMLNode* pXMLDocumentNode,
                                     XFA_XDPPACKET ePacketID);
  CXFA_Node* ParseAsXDPPacket_TemplateForm(CFDE_XMLNode* pXMLDocumentNode,
                                           XFA_XDPPACKET ePacketID);
  CXFA_Node* ParseAsXDPPacket_Data(CFDE_XMLNode* pXMLDocumentNode,
                                   XFA_XDPPACKET ePacketID);
  CXFA_Node* ParseAsXDPPacket_LocaleConnectionSourceSet(
      CFDE_XMLNode* pXMLDocumentNode,
      XFA_XDPPACKET ePacketID);
  CXFA_Node* ParseAsXDPPacket_Xdc(CFDE_XMLNode* pXMLDocumentNode,
                                  XFA_XDPPACKET ePacketID);
  CXFA_Node* ParseAsXDPPacket_User(CFDE_XMLNode* pXMLDocumentNode,
                                   XFA_XDPPACKET ePacketID);
  CXFA_Node* NormalLoader(CXFA_Node* pXFANode,
                          CFDE_XMLNode* pXMLDoc,
                          XFA_XDPPACKET ePacketID,
                          FX_BOOL bUseAttribute = TRUE);
  CXFA_Node* DataLoader(CXFA_Node* pXFANode,
                        CFDE_XMLNode* pXMLDoc,
                        FX_BOOL bDoTransform);
  CXFA_Node* UserPacketLoader(CXFA_Node* pXFANode, CFDE_XMLNode* pXMLDoc);
  void ParseContentNode(CXFA_Node* pXFANode,
                        CFDE_XMLNode* pXMLNode,
                        XFA_XDPPACKET ePacketID);
  void ParseDataValue(CXFA_Node* pXFANode,
                      CFDE_XMLNode* pXMLNode,
                      XFA_XDPPACKET ePacketID);
  void ParseDataGroup(CXFA_Node* pXFANode,
                      CFDE_XMLNode* pXMLNode,
                      XFA_XDPPACKET ePacketID);
  void ParseInstruction(CXFA_Node* pXFANode,
                        CFDE_XMLInstruction* pXMLInstruction,
                        XFA_XDPPACKET ePacketID);
  void SetFactory(CXFA_Document* pFactory);

  CXFA_XMLParser* m_pXMLParser;
  CFDE_XMLDoc* m_pXMLDoc;
  IFX_Stream* m_pStream;
  IFX_FileRead* m_pFileRead;
  CXFA_Document* m_pFactory;
  CXFA_Node* m_pRootNode;
  XFA_XDPPACKET m_ePacketID;
  FX_BOOL m_bDocumentParser;
  friend class CXFA_DocumentParser;
};

class CXFA_DocumentParser : public IXFA_Parser {
 public:
  CXFA_DocumentParser(CXFA_FFNotify* pNotify);
  ~CXFA_DocumentParser() override;

  // IXFA_Parser
  void Release() override;
  int32_t StartParse(IFX_FileRead* pStream,
                     XFA_XDPPACKET ePacketID = XFA_XDPPACKET_XDP) override;
  int32_t DoParse(IFX_Pause* pPause = nullptr) override;
  int32_t ParseXMLData(const CFX_WideString& wsXML,
                       CFDE_XMLNode*& pXMLNode,
                       IFX_Pause* pPause = nullptr) override;
  void ConstructXFANode(CXFA_Node* pXFANode, CFDE_XMLNode* pXMLNode) override;
  CXFA_Document* GetFactory() const override;
  CXFA_Node* GetRootNode() const override;
  CFDE_XMLDoc* GetXMLDoc() const override;
  CXFA_FFNotify* GetNotify() const;
  CXFA_Document* GetDocument() const;
  void CloseParser() override;

 protected:
  CXFA_SimpleParser m_nodeParser;
  CXFA_FFNotify* m_pNotify;
  CXFA_Document* m_pDocument;
};

class CXFA_XMLParser : public CFDE_XMLParser {
 public:
  CXFA_XMLParser(CFDE_XMLNode* pRoot, IFX_Stream* pStream);
  ~CXFA_XMLParser() override;

  // CFDE_XMLParser
  void Release() override;
  int32_t DoParser(IFX_Pause* pPause) override;

  FX_FILESIZE m_nStart[2];
  size_t m_nSize[2];
  FX_FILESIZE m_nElementStart;
  uint16_t m_dwCheckStatus;
  uint16_t m_dwCurrentCheckStatus;

 protected:
  CFDE_XMLNode* m_pRoot;
  IFX_Stream* m_pStream;
  CFDE_XMLSyntaxParser* m_pParser;
  CFDE_XMLNode* m_pParent;
  CFDE_XMLNode* m_pChild;
  CFX_StackTemplate<CFDE_XMLNode*> m_NodeStack;
  CFX_WideString m_ws1;
  CFX_WideString m_ws2;
  FDE_XmlSyntaxResult m_syntaxParserResult;
};

#endif  // XFA_FXFA_PARSER_XFA_PARSER_IMP_H_
