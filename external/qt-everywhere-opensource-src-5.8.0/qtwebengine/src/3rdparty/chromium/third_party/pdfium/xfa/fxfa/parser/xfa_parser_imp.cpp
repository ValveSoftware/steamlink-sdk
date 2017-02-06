// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/xfa_parser_imp.h"

#include <memory>

#include "xfa/fde/xml/fde_xml_imp.h"
#include "xfa/fgas/crt/fgas_codepage.h"
#include "xfa/fxfa/include/xfa_checksum.h"
#include "xfa/fxfa/parser/xfa_basic_imp.h"
#include "xfa/fxfa/parser/xfa_doclayout.h"
#include "xfa/fxfa/parser/xfa_document.h"
#include "xfa/fxfa/parser/xfa_localemgr.h"
#include "xfa/fxfa/parser/xfa_object.h"
#include "xfa/fxfa/parser/xfa_parser.h"
#include "xfa/fxfa/parser/xfa_script.h"
#include "xfa/fxfa/parser/xfa_utils.h"

IXFA_Parser* IXFA_Parser::Create(CXFA_Document* pFactory,
                                 FX_BOOL bDocumentParser) {
  return new CXFA_SimpleParser(pFactory, bDocumentParser);
}
CXFA_SimpleParser::CXFA_SimpleParser(CXFA_Document* pFactory,
                                     FX_BOOL bDocumentParser)
    : m_pXMLParser(nullptr),
      m_pXMLDoc(nullptr),
      m_pStream(nullptr),
      m_pFileRead(nullptr),
      m_pFactory(pFactory),
      m_pRootNode(nullptr),
      m_ePacketID(XFA_XDPPACKET_UNKNOWN),
      m_bDocumentParser(bDocumentParser) {}
CXFA_SimpleParser::~CXFA_SimpleParser() {
  CloseParser();
}
void CXFA_SimpleParser::Release() {
  delete this;
}
void CXFA_SimpleParser::SetFactory(CXFA_Document* pFactory) {
  m_pFactory = pFactory;
}
static CFDE_XMLNode* XFA_FDEExtension_GetDocumentNode(
    CFDE_XMLDoc* pXMLDoc,
    FX_BOOL bVerifyWellFormness = FALSE) {
  if (!pXMLDoc) {
    return nullptr;
  }
  CFDE_XMLNode* pXMLFakeRoot = pXMLDoc->GetRoot();
  for (CFDE_XMLNode* pXMLNode =
           pXMLFakeRoot->GetNodeItem(CFDE_XMLNode::FirstChild);
       pXMLNode; pXMLNode = pXMLNode->GetNodeItem(CFDE_XMLNode::NextSibling)) {
    if (pXMLNode->GetType() == FDE_XMLNODE_Element) {
      if (bVerifyWellFormness) {
        for (CFDE_XMLNode* pNextNode =
                 pXMLNode->GetNodeItem(CFDE_XMLNode::NextSibling);
             pNextNode;
             pNextNode = pNextNode->GetNodeItem(CFDE_XMLNode::NextSibling)) {
          if (pNextNode->GetType() == FDE_XMLNODE_Element) {
            return FALSE;
          }
        }
      }
      return pXMLNode;
    }
  }
  return nullptr;
}
int32_t CXFA_SimpleParser::StartParse(IFX_FileRead* pStream,
                                      XFA_XDPPACKET ePacketID) {
  CloseParser();
  m_pFileRead = pStream;
  m_pStream = IFX_Stream::CreateStream(
      pStream, FX_STREAMACCESS_Read | FX_STREAMACCESS_Text);
  if (!m_pStream) {
    return XFA_PARSESTATUS_StreamErr;
  }
  uint16_t wCodePage = m_pStream->GetCodePage();
  if (wCodePage != FX_CODEPAGE_UTF16LE && wCodePage != FX_CODEPAGE_UTF16BE &&
      wCodePage != FX_CODEPAGE_UTF8) {
    m_pStream->SetCodePage(FX_CODEPAGE_UTF8);
  }
  m_pXMLDoc = new CFDE_XMLDoc;
  m_pXMLParser = new CXFA_XMLParser(m_pXMLDoc->GetRoot(), m_pStream);
  if (!m_pXMLDoc->LoadXML(m_pXMLParser)) {
    return XFA_PARSESTATUS_StatusErr;
  }
  m_ePacketID = ePacketID;
  return XFA_PARSESTATUS_Ready;
}
int32_t CXFA_SimpleParser::DoParse(IFX_Pause* pPause) {
  if (!m_pXMLDoc || m_ePacketID == XFA_XDPPACKET_UNKNOWN) {
    return XFA_PARSESTATUS_StatusErr;
  }
  int32_t iRet = m_pXMLDoc->DoLoad(pPause);
  if (iRet < 0) {
    return XFA_PARSESTATUS_SyntaxErr;
  }
  if (iRet < 100) {
    return iRet / 2;
  }
  m_pRootNode = ParseAsXDPPacket(XFA_FDEExtension_GetDocumentNode(m_pXMLDoc),
                                 m_ePacketID);
  m_pXMLDoc->CloseXML();
  if (m_pStream) {
    m_pStream->Release();
    m_pStream = nullptr;
  }
  if (!m_pRootNode) {
    return XFA_PARSESTATUS_StatusErr;
  }
  return XFA_PARSESTATUS_Done;
}
int32_t CXFA_SimpleParser::ParseXMLData(const CFX_WideString& wsXML,
                                        CFDE_XMLNode*& pXMLNode,
                                        IFX_Pause* pPause) {
  CloseParser();
  pXMLNode = nullptr;
  IFX_Stream* pStream = XFA_CreateWideTextRead(wsXML);
  if (!pStream) {
    return XFA_PARSESTATUS_StreamErr;
  }
  m_pStream = pStream;
  m_pXMLDoc = new CFDE_XMLDoc;
  CXFA_XMLParser* pParser = new CXFA_XMLParser(m_pXMLDoc->GetRoot(), m_pStream);
  pParser->m_dwCheckStatus = 0x03;
  if (!m_pXMLDoc->LoadXML(pParser)) {
    return XFA_PARSESTATUS_StatusErr;
  }
  int32_t iRet = m_pXMLDoc->DoLoad(pPause);
  if (iRet < 0 || iRet >= 100) {
    m_pXMLDoc->CloseXML();
  }
  if (iRet < 0) {
    return XFA_PARSESTATUS_SyntaxErr;
  }
  if (iRet < 100) {
    return iRet / 2;
  }
  if (m_pStream) {
    m_pStream->Release();
    m_pStream = nullptr;
  }
  pXMLNode = XFA_FDEExtension_GetDocumentNode(m_pXMLDoc);
  return XFA_PARSESTATUS_Done;
}

void CXFA_SimpleParser::ConstructXFANode(CXFA_Node* pXFANode,
                                         CFDE_XMLNode* pXMLNode) {
  XFA_XDPPACKET ePacketID = (XFA_XDPPACKET)pXFANode->GetPacketID();
  if (ePacketID == XFA_XDPPACKET_Datasets) {
    if (pXFANode->GetElementType() == XFA_Element::DataValue) {
      for (CFDE_XMLNode* pXMLChild =
               pXMLNode->GetNodeItem(CFDE_XMLNode::FirstChild);
           pXMLChild;
           pXMLChild = pXMLChild->GetNodeItem(CFDE_XMLNode::NextSibling)) {
        FDE_XMLNODETYPE eNodeType = pXMLChild->GetType();
        if (eNodeType == FDE_XMLNODE_Instruction)
          continue;

        if (eNodeType == FDE_XMLNODE_Element) {
          CXFA_Node* pXFAChild = m_pFactory->CreateNode(XFA_XDPPACKET_Datasets,
                                                        XFA_Element::DataValue);
          if (!pXFAChild)
            return;

          CFX_WideString wsNodeStr;
          CFDE_XMLElement* child = static_cast<CFDE_XMLElement*>(pXMLChild);
          child->GetLocalTagName(wsNodeStr);
          pXFAChild->SetCData(XFA_ATTRIBUTE_Name, wsNodeStr);
          CFX_WideString wsChildValue;
          XFA_GetPlainTextFromRichText(child, wsChildValue);
          if (!wsChildValue.IsEmpty())
            pXFAChild->SetCData(XFA_ATTRIBUTE_Value, wsChildValue);

          pXFANode->InsertChild(pXFAChild);
          pXFAChild->SetXMLMappingNode(pXMLChild);
          pXFAChild->SetFlag(XFA_NodeFlag_Initialized, false);
          break;
        }
      }
      m_pRootNode = pXFANode;
    } else {
      m_pRootNode = DataLoader(pXFANode, pXMLNode, TRUE);
    }
  } else if (pXFANode->IsContentNode()) {
    ParseContentNode(pXFANode, pXMLNode, ePacketID);
    m_pRootNode = pXFANode;
  } else {
    m_pRootNode = NormalLoader(pXFANode, pXMLNode, ePacketID);
  }
}

CXFA_Document* CXFA_SimpleParser::GetFactory() const {
  return m_pFactory;
}

CXFA_Node* CXFA_SimpleParser::GetRootNode() const {
  return m_pRootNode;
}

CFDE_XMLDoc* CXFA_SimpleParser::GetXMLDoc() const {
  return m_pXMLDoc;
}

FX_BOOL XFA_FDEExtension_ResolveNamespaceQualifier(
    CFDE_XMLElement* pNode,
    const CFX_WideStringC& wsQualifier,
    CFX_WideString& wsNamespaceURI) {
  if (!pNode) {
    return FALSE;
  }
  CFDE_XMLNode* pFakeRoot = pNode->GetNodeItem(CFDE_XMLNode::Root);
  CFX_WideString wsNSAttribute;
  FX_BOOL bRet = FALSE;
  if (wsQualifier.IsEmpty()) {
    wsNSAttribute = FX_WSTRC(L"xmlns");
    bRet = TRUE;
  } else {
    wsNSAttribute = FX_WSTRC(L"xmlns:") + wsQualifier;
  }
  for (; pNode != pFakeRoot; pNode = static_cast<CFDE_XMLElement*>(
                                 pNode->GetNodeItem(CFDE_XMLNode::Parent))) {
    if (pNode->GetType() != FDE_XMLNODE_Element) {
      continue;
    }
    if (pNode->HasAttribute(wsNSAttribute.c_str())) {
      pNode->GetString(wsNSAttribute.c_str(), wsNamespaceURI);
      return TRUE;
    }
  }
  wsNamespaceURI.clear();
  return bRet;
}
static inline void XFA_FDEExtension_GetElementTagNamespaceURI(
    CFDE_XMLElement* pElement,
    CFX_WideString& wsNamespaceURI) {
  CFX_WideString wsNodeStr;
  pElement->GetNamespacePrefix(wsNodeStr);
  if (!XFA_FDEExtension_ResolveNamespaceQualifier(
          pElement, wsNodeStr.AsStringC(), wsNamespaceURI)) {
    wsNamespaceURI.clear();
  }
}
static FX_BOOL XFA_FDEExtension_MatchNodeName(
    CFDE_XMLNode* pNode,
    const CFX_WideStringC& wsLocalTagName,
    const CFX_WideStringC& wsNamespaceURIPrefix,
    uint32_t eMatchFlags = XFA_XDPPACKET_FLAGS_NOMATCH) {
  if (!pNode || pNode->GetType() != FDE_XMLNODE_Element) {
    return FALSE;
  }
  CFDE_XMLElement* pElement = reinterpret_cast<CFDE_XMLElement*>(pNode);
  CFX_WideString wsNodeStr;
  pElement->GetLocalTagName(wsNodeStr);
  if (wsNodeStr != wsLocalTagName) {
    return FALSE;
  }
  XFA_FDEExtension_GetElementTagNamespaceURI(pElement, wsNodeStr);
  if (eMatchFlags & XFA_XDPPACKET_FLAGS_NOMATCH) {
    return TRUE;
  }
  if (eMatchFlags & XFA_XDPPACKET_FLAGS_PREFIXMATCH) {
    return wsNodeStr.Left(wsNamespaceURIPrefix.GetLength()) ==
           wsNamespaceURIPrefix;
  }
  return wsNodeStr == wsNamespaceURIPrefix;
}
static FX_BOOL XFA_FDEExtension_GetAttributeLocalName(
    const CFX_WideStringC& wsAttributeName,
    CFX_WideString& wsLocalAttrName) {
  CFX_WideString wsAttrName(wsAttributeName);
  FX_STRSIZE iFind = wsAttrName.Find(L':', 0);
  if (iFind < 0) {
    wsLocalAttrName = wsAttrName;
    return FALSE;
  } else {
    wsLocalAttrName = wsAttrName.Right(wsAttrName.GetLength() - iFind - 1);
    return TRUE;
  }
}
static FX_BOOL XFA_FDEExtension_ResolveAttribute(
    CFDE_XMLElement* pElement,
    const CFX_WideStringC& wsAttributeName,
    CFX_WideString& wsLocalAttrName,
    CFX_WideString& wsNamespaceURI) {
  CFX_WideString wsAttrName(wsAttributeName);
  CFX_WideString wsNSPrefix;
  if (XFA_FDEExtension_GetAttributeLocalName(wsAttributeName,
                                             wsLocalAttrName)) {
    wsNSPrefix = wsAttrName.Left(wsAttributeName.GetLength() -
                                 wsLocalAttrName.GetLength() - 1);
  }
  if (wsLocalAttrName == FX_WSTRC(L"xmlns") ||
      wsNSPrefix == FX_WSTRC(L"xmlns") || wsNSPrefix == FX_WSTRC(L"xml")) {
    return FALSE;
  }
  if (!XFA_FDEExtension_ResolveNamespaceQualifier(
          pElement, wsNSPrefix.AsStringC(), wsNamespaceURI)) {
    wsNamespaceURI.clear();
    return FALSE;
  }
  return TRUE;
}
static FX_BOOL XFA_FDEExtension_FindAttributeWithNS(
    CFDE_XMLElement* pElement,
    const CFX_WideStringC& wsLocalAttributeName,
    const CFX_WideStringC& wsNamespaceURIPrefix,
    CFX_WideString& wsValue,
    FX_BOOL bMatchNSAsPrefix = FALSE) {
  if (!pElement) {
    return FALSE;
  }
  CFX_WideString wsAttrName;
  CFX_WideString wsAttrValue;
  CFX_WideString wsAttrNS;
  for (int32_t iAttrCount = pElement->CountAttributes(), i = 0; i < iAttrCount;
       i++) {
    pElement->GetAttribute(i, wsAttrName, wsAttrValue);
    FX_STRSIZE iFind = wsAttrName.Find(L':', 0);
    CFX_WideString wsNSPrefix;
    if (iFind < 0) {
      if (wsLocalAttributeName != wsAttrName) {
        continue;
      }
    } else {
      if (wsLocalAttributeName !=
          wsAttrName.Right(wsAttrName.GetLength() - iFind - 1)) {
        continue;
      }
      wsNSPrefix = wsAttrName.Left(iFind);
    }
    if (!XFA_FDEExtension_ResolveNamespaceQualifier(
            pElement, wsNSPrefix.AsStringC(), wsAttrNS)) {
      continue;
    }
    if (bMatchNSAsPrefix) {
      if (wsAttrNS.Left(wsNamespaceURIPrefix.GetLength()) !=
          wsNamespaceURIPrefix) {
        continue;
      }
    } else {
      if (wsAttrNS != wsNamespaceURIPrefix) {
        continue;
      }
    }
    wsValue = wsAttrValue;
    return TRUE;
  }
  return FALSE;
}
CXFA_Node* CXFA_SimpleParser::ParseAsXDPPacket(CFDE_XMLNode* pXMLDocumentNode,
                                               XFA_XDPPACKET ePacketID) {
  switch (ePacketID) {
    case XFA_XDPPACKET_UNKNOWN:
      return nullptr;
    case XFA_XDPPACKET_XDP:
      return ParseAsXDPPacket_XDP(pXMLDocumentNode, ePacketID);
    case XFA_XDPPACKET_Config:
      return ParseAsXDPPacket_Config(pXMLDocumentNode, ePacketID);
    case XFA_XDPPACKET_Template:
    case XFA_XDPPACKET_Form:
      return ParseAsXDPPacket_TemplateForm(pXMLDocumentNode, ePacketID);
    case XFA_XDPPACKET_Datasets:
      return ParseAsXDPPacket_Data(pXMLDocumentNode, ePacketID);
    case XFA_XDPPACKET_Xdc:
      return ParseAsXDPPacket_Xdc(pXMLDocumentNode, ePacketID);
    case XFA_XDPPACKET_LocaleSet:
    case XFA_XDPPACKET_ConnectionSet:
    case XFA_XDPPACKET_SourceSet:
      return ParseAsXDPPacket_LocaleConnectionSourceSet(pXMLDocumentNode,
                                                        ePacketID);
    default:
      return ParseAsXDPPacket_User(pXMLDocumentNode, ePacketID);
  }
}
CXFA_Node* CXFA_SimpleParser::ParseAsXDPPacket_XDP(
    CFDE_XMLNode* pXMLDocumentNode,
    XFA_XDPPACKET ePacketID) {
  if (!XFA_FDEExtension_MatchNodeName(
          pXMLDocumentNode, XFA_GetPacketByIndex(XFA_PACKET_XDP)->pName,
          XFA_GetPacketByIndex(XFA_PACKET_XDP)->pURI,
          XFA_GetPacketByIndex(XFA_PACKET_XDP)->eFlags)) {
    return nullptr;
  }
  CXFA_Node* pXFARootNode =
      m_pFactory->CreateNode(XFA_XDPPACKET_XDP, XFA_Element::Xfa);
  if (!pXFARootNode) {
    return nullptr;
  }
  m_pRootNode = pXFARootNode;
  pXFARootNode->SetCData(XFA_ATTRIBUTE_Name, L"xfa");
  {
    CFDE_XMLElement* pElement = static_cast<CFDE_XMLElement*>(pXMLDocumentNode);
    int32_t iAttributeCount = pElement->CountAttributes();
    for (int32_t i = 0; i < iAttributeCount; i++) {
      CFX_WideString wsAttriName, wsAttriValue;
      pElement->GetAttribute(i, wsAttriName, wsAttriValue);
      if (wsAttriName == FX_WSTRC(L"uuid")) {
        pXFARootNode->SetCData(XFA_ATTRIBUTE_Uuid, wsAttriValue);
      } else if (wsAttriName == FX_WSTRC(L"timeStamp")) {
        pXFARootNode->SetCData(XFA_ATTRIBUTE_TimeStamp, wsAttriValue);
      }
    }
  }
  CFDE_XMLNode* pXMLConfigDOMRoot = nullptr;
  CXFA_Node* pXFAConfigDOMRoot = nullptr;
  {
    for (CFDE_XMLNode* pChildItem =
             pXMLDocumentNode->GetNodeItem(CFDE_XMLNode::FirstChild);
         pChildItem;
         pChildItem = pChildItem->GetNodeItem(CFDE_XMLNode::NextSibling)) {
      const XFA_PACKETINFO* pPacketInfo =
          XFA_GetPacketByIndex(XFA_PACKET_Config);
      if (!XFA_FDEExtension_MatchNodeName(pChildItem, pPacketInfo->pName,
                                          pPacketInfo->pURI,
                                          pPacketInfo->eFlags)) {
        continue;
      }
      if (pXFARootNode->GetFirstChildByName(pPacketInfo->uHash)) {
        return nullptr;
      }
      pXMLConfigDOMRoot = pChildItem;
      pXFAConfigDOMRoot =
          ParseAsXDPPacket_Config(pXMLConfigDOMRoot, XFA_XDPPACKET_Config);
      pXFARootNode->InsertChild(pXFAConfigDOMRoot, nullptr);
    }
  }
  CFDE_XMLNode* pXMLDatasetsDOMRoot = nullptr;
  CFDE_XMLNode* pXMLFormDOMRoot = nullptr;
  CFDE_XMLNode* pXMLTemplateDOMRoot = nullptr;
  {
    for (CFDE_XMLNode* pChildItem =
             pXMLDocumentNode->GetNodeItem(CFDE_XMLNode::FirstChild);
         pChildItem;
         pChildItem = pChildItem->GetNodeItem(CFDE_XMLNode::NextSibling)) {
      if (!pChildItem || pChildItem->GetType() != FDE_XMLNODE_Element) {
        continue;
      }
      if (pChildItem == pXMLConfigDOMRoot) {
        continue;
      }
      CFDE_XMLElement* pElement =
          reinterpret_cast<CFDE_XMLElement*>(pChildItem);
      CFX_WideString wsPacketName;
      pElement->GetLocalTagName(wsPacketName);
      const XFA_PACKETINFO* pPacketInfo =
          XFA_GetPacketByName(wsPacketName.AsStringC());
      if (pPacketInfo && pPacketInfo->pURI) {
        if (!XFA_FDEExtension_MatchNodeName(pElement, pPacketInfo->pName,
                                            pPacketInfo->pURI,
                                            pPacketInfo->eFlags)) {
          pPacketInfo = nullptr;
        }
      }
      XFA_XDPPACKET ePacket =
          pPacketInfo ? pPacketInfo->eName : XFA_XDPPACKET_USER;
      if (ePacket == XFA_XDPPACKET_XDP) {
        continue;
      }
      if (ePacket == XFA_XDPPACKET_Datasets) {
        if (pXMLDatasetsDOMRoot) {
          return nullptr;
        }
        pXMLDatasetsDOMRoot = pElement;
      } else if (ePacket == XFA_XDPPACKET_Form) {
        if (pXMLFormDOMRoot) {
          return nullptr;
        }
        pXMLFormDOMRoot = pElement;
      } else if (ePacket == XFA_XDPPACKET_Template) {
        if (pXMLTemplateDOMRoot) {
          // Found a duplicate template packet.
          return nullptr;
        }
        CXFA_Node* pPacketNode = ParseAsXDPPacket(pElement, ePacket);
        if (pPacketNode) {
          pXMLTemplateDOMRoot = pElement;
          pXFARootNode->InsertChild(pPacketNode);
        }
      } else {
        CXFA_Node* pPacketNode = ParseAsXDPPacket(pElement, ePacket);
        if (pPacketNode) {
          if (pPacketInfo &&
              (pPacketInfo->eFlags & XFA_XDPPACKET_FLAGS_SUPPORTONE) &&
              pXFARootNode->GetFirstChildByName(pPacketInfo->uHash)) {
            return nullptr;
          }
          pXFARootNode->InsertChild(pPacketNode);
        }
      }
    }
  }
  if (!pXMLTemplateDOMRoot) {
    // No template is found.
    return nullptr;
  }
  if (pXMLDatasetsDOMRoot) {
    CXFA_Node* pPacketNode =
        ParseAsXDPPacket(pXMLDatasetsDOMRoot, XFA_XDPPACKET_Datasets);
    if (pPacketNode) {
      pXFARootNode->InsertChild(pPacketNode);
    }
  }
  if (pXMLFormDOMRoot) {
    CXFA_Node* pPacketNode =
        ParseAsXDPPacket(pXMLFormDOMRoot, XFA_XDPPACKET_Form);
    if (pPacketNode) {
      pXFARootNode->InsertChild(pPacketNode);
    }
  }
  pXFARootNode->SetXMLMappingNode(pXMLDocumentNode);
  return pXFARootNode;
}
CXFA_Node* CXFA_SimpleParser::ParseAsXDPPacket_Config(
    CFDE_XMLNode* pXMLDocumentNode,
    XFA_XDPPACKET ePacketID) {
  if (!XFA_FDEExtension_MatchNodeName(
          pXMLDocumentNode, XFA_GetPacketByIndex(XFA_PACKET_Config)->pName,
          XFA_GetPacketByIndex(XFA_PACKET_Config)->pURI,
          XFA_GetPacketByIndex(XFA_PACKET_Config)->eFlags)) {
    return nullptr;
  }
  CXFA_Node* pNode =
      m_pFactory->CreateNode(XFA_XDPPACKET_Config, XFA_Element::Config);
  if (!pNode) {
    return nullptr;
  }
  pNode->SetCData(XFA_ATTRIBUTE_Name,
                  XFA_GetPacketByIndex(XFA_PACKET_Config)->pName);
  if (!NormalLoader(pNode, pXMLDocumentNode, ePacketID)) {
    return nullptr;
  }
  pNode->SetXMLMappingNode(pXMLDocumentNode);
  return pNode;
}
CXFA_Node* CXFA_SimpleParser::ParseAsXDPPacket_TemplateForm(
    CFDE_XMLNode* pXMLDocumentNode,
    XFA_XDPPACKET ePacketID) {
  CXFA_Node* pNode = nullptr;
  if (ePacketID == XFA_XDPPACKET_Template) {
    if (XFA_FDEExtension_MatchNodeName(
            pXMLDocumentNode, XFA_GetPacketByIndex(XFA_PACKET_Template)->pName,
            XFA_GetPacketByIndex(XFA_PACKET_Template)->pURI,
            XFA_GetPacketByIndex(XFA_PACKET_Template)->eFlags)) {
      pNode =
          m_pFactory->CreateNode(XFA_XDPPACKET_Template, XFA_Element::Template);
      if (!pNode) {
        return nullptr;
      }
      pNode->SetCData(XFA_ATTRIBUTE_Name,
                      XFA_GetPacketByIndex(XFA_PACKET_Template)->pName);
      if (m_bDocumentParser) {
        CFX_WideString wsNamespaceURI;
        CFDE_XMLElement* pXMLDocumentElement =
            static_cast<CFDE_XMLElement*>(pXMLDocumentNode);
        pXMLDocumentElement->GetNamespaceURI(wsNamespaceURI);
        if (wsNamespaceURI.IsEmpty()) {
          pXMLDocumentElement->GetString(L"xmlns:xfa", wsNamespaceURI);
        }
        pNode->GetDocument()->RecognizeXFAVersionNumber(wsNamespaceURI);
      }
      if (!NormalLoader(pNode, pXMLDocumentNode, ePacketID)) {
        return nullptr;
      }
    }
  } else if (ePacketID == XFA_XDPPACKET_Form) {
    if (XFA_FDEExtension_MatchNodeName(
            pXMLDocumentNode, XFA_GetPacketByIndex(XFA_PACKET_Form)->pName,
            XFA_GetPacketByIndex(XFA_PACKET_Form)->pURI,
            XFA_GetPacketByIndex(XFA_PACKET_Form)->eFlags)) {
      CFDE_XMLElement* pXMLDocumentElement =
          static_cast<CFDE_XMLElement*>(pXMLDocumentNode);
      CFX_WideString wsChecksum;
      pXMLDocumentElement->GetString(L"checksum", wsChecksum);
      if (wsChecksum.GetLength() != 28 ||
          m_pXMLParser->m_dwCheckStatus != 0x03) {
        return nullptr;
      }
      std::unique_ptr<CXFA_ChecksumContext> pChecksum(new CXFA_ChecksumContext);
      pChecksum->StartChecksum();
      pChecksum->UpdateChecksum(m_pFileRead, m_pXMLParser->m_nStart[0],
                                m_pXMLParser->m_nSize[0]);
      pChecksum->UpdateChecksum(m_pFileRead, m_pXMLParser->m_nStart[1],
                                m_pXMLParser->m_nSize[1]);
      pChecksum->FinishChecksum();
      CFX_ByteString bsCheck = pChecksum->GetChecksum();
      if (bsCheck != wsChecksum.UTF8Encode())
        return nullptr;

      pNode = m_pFactory->CreateNode(XFA_XDPPACKET_Form, XFA_Element::Form);
      if (!pNode)
        return nullptr;

      pNode->SetCData(XFA_ATTRIBUTE_Name,
                      XFA_GetPacketByIndex(XFA_PACKET_Form)->pName);
      pNode->SetAttribute(XFA_ATTRIBUTE_Checksum, wsChecksum.AsStringC());
      CXFA_Node* pTemplateRoot =
          m_pRootNode->GetFirstChildByClass(XFA_Element::Template);
      CXFA_Node* pTemplateChosen =
          pTemplateRoot
              ? pTemplateRoot->GetFirstChildByClass(XFA_Element::Subform)
              : nullptr;
      FX_BOOL bUseAttribute = TRUE;
      if (pTemplateChosen &&
          pTemplateChosen->GetEnum(XFA_ATTRIBUTE_RestoreState) !=
              XFA_ATTRIBUTEENUM_Auto) {
        bUseAttribute = FALSE;
      }
      if (!NormalLoader(pNode, pXMLDocumentNode, ePacketID, bUseAttribute)) {
        return nullptr;
      }
    }
  }
  if (pNode) {
    pNode->SetXMLMappingNode(pXMLDocumentNode);
  }
  return pNode;
}
static CFDE_XMLNode* XFA_GetDataSetsFromXDP(CFDE_XMLNode* pXMLDocumentNode) {
  if (XFA_FDEExtension_MatchNodeName(
          pXMLDocumentNode, XFA_GetPacketByIndex(XFA_PACKET_Datasets)->pName,
          XFA_GetPacketByIndex(XFA_PACKET_Datasets)->pURI,
          XFA_GetPacketByIndex(XFA_PACKET_Datasets)->eFlags)) {
    return pXMLDocumentNode;
  }
  if (!XFA_FDEExtension_MatchNodeName(
          pXMLDocumentNode, XFA_GetPacketByIndex(XFA_PACKET_XDP)->pName,
          XFA_GetPacketByIndex(XFA_PACKET_XDP)->pURI,
          XFA_GetPacketByIndex(XFA_PACKET_XDP)->eFlags)) {
    return nullptr;
  }
  for (CFDE_XMLNode* pDatasetsNode =
           pXMLDocumentNode->GetNodeItem(CFDE_XMLNode::FirstChild);
       pDatasetsNode;
       pDatasetsNode = pDatasetsNode->GetNodeItem(CFDE_XMLNode::NextSibling)) {
    if (!XFA_FDEExtension_MatchNodeName(
            pDatasetsNode, XFA_GetPacketByIndex(XFA_PACKET_Datasets)->pName,
            XFA_GetPacketByIndex(XFA_PACKET_Datasets)->pURI,
            XFA_GetPacketByIndex(XFA_PACKET_Datasets)->eFlags)) {
      continue;
    }
    return pDatasetsNode;
  }
  return nullptr;
}
CXFA_Node* CXFA_SimpleParser::ParseAsXDPPacket_Data(
    CFDE_XMLNode* pXMLDocumentNode,
    XFA_XDPPACKET ePacketID) {
  CFDE_XMLNode* pDatasetsXMLNode = XFA_GetDataSetsFromXDP(pXMLDocumentNode);
  if (pDatasetsXMLNode) {
    CXFA_Node* pNode =
        m_pFactory->CreateNode(XFA_XDPPACKET_Datasets, XFA_Element::DataModel);
    if (!pNode) {
      return nullptr;
    }
    pNode->SetCData(XFA_ATTRIBUTE_Name,
                    XFA_GetPacketByIndex(XFA_PACKET_Datasets)->pName);
    if (!DataLoader(pNode, pDatasetsXMLNode, FALSE)) {
      return nullptr;
    }
    pNode->SetXMLMappingNode(pDatasetsXMLNode);
    return pNode;
  }
  CFDE_XMLNode* pDataXMLNode = nullptr;
  if (XFA_FDEExtension_MatchNodeName(
          pXMLDocumentNode, FX_WSTRC(L"data"),
          XFA_GetPacketByIndex(XFA_PACKET_Datasets)->pURI,
          XFA_GetPacketByIndex(XFA_PACKET_Datasets)->eFlags)) {
    static_cast<CFDE_XMLElement*>(pXMLDocumentNode)
        ->RemoveAttribute(L"xmlns:xfa");
    pDataXMLNode = pXMLDocumentNode;
  } else {
    CFDE_XMLElement* pDataElement = new CFDE_XMLElement(L"xfa:data");
    CFDE_XMLNode* pParentXMLNode =
        pXMLDocumentNode->GetNodeItem(CFDE_XMLNode::Parent);
    if (pParentXMLNode) {
      pParentXMLNode->RemoveChildNode(pXMLDocumentNode);
    }
    ASSERT(pXMLDocumentNode->GetType() == FDE_XMLNODE_Element);
    if (pXMLDocumentNode->GetType() == FDE_XMLNODE_Element) {
      static_cast<CFDE_XMLElement*>(pXMLDocumentNode)
          ->RemoveAttribute(L"xmlns:xfa");
    }
    pDataElement->InsertChildNode(pXMLDocumentNode);
    pDataXMLNode = pDataElement;
  }
  if (pDataXMLNode) {
    CXFA_Node* pNode =
        m_pFactory->CreateNode(XFA_XDPPACKET_Datasets, XFA_Element::DataGroup);
    if (!pNode) {
      if (pDataXMLNode != pXMLDocumentNode) {
        pDataXMLNode->Release();
      }
      return nullptr;
    }
    CFX_WideString wsLocalName;
    static_cast<CFDE_XMLElement*>(pDataXMLNode)->GetLocalTagName(wsLocalName);
    pNode->SetCData(XFA_ATTRIBUTE_Name, wsLocalName);
    if (!DataLoader(pNode, pDataXMLNode, TRUE)) {
      return nullptr;
    }
    pNode->SetXMLMappingNode(pDataXMLNode);
    if (pDataXMLNode != pXMLDocumentNode) {
      pNode->SetFlag(XFA_NodeFlag_OwnXMLNode, false);
    }
    return pNode;
  }
  return nullptr;
}
CXFA_Node* CXFA_SimpleParser::ParseAsXDPPacket_LocaleConnectionSourceSet(
    CFDE_XMLNode* pXMLDocumentNode,
    XFA_XDPPACKET ePacketID) {
  CXFA_Node* pNode = nullptr;
  if (ePacketID == XFA_XDPPACKET_LocaleSet) {
    if (XFA_FDEExtension_MatchNodeName(
            pXMLDocumentNode, XFA_GetPacketByIndex(XFA_PACKET_LocaleSet)->pName,
            XFA_GetPacketByIndex(XFA_PACKET_LocaleSet)->pURI,
            XFA_GetPacketByIndex(XFA_PACKET_LocaleSet)->eFlags)) {
      pNode = m_pFactory->CreateNode(XFA_XDPPACKET_LocaleSet,
                                     XFA_Element::LocaleSet);
      if (!pNode) {
        return nullptr;
      }
      pNode->SetCData(XFA_ATTRIBUTE_Name,
                      XFA_GetPacketByIndex(XFA_PACKET_LocaleSet)->pName);
      if (!NormalLoader(pNode, pXMLDocumentNode, ePacketID)) {
        return nullptr;
      }
    }
  } else if (ePacketID == XFA_XDPPACKET_ConnectionSet) {
    if (XFA_FDEExtension_MatchNodeName(
            pXMLDocumentNode,
            XFA_GetPacketByIndex(XFA_PACKET_ConnectionSet)->pName,
            XFA_GetPacketByIndex(XFA_PACKET_ConnectionSet)->pURI,
            XFA_GetPacketByIndex(XFA_PACKET_ConnectionSet)->eFlags)) {
      pNode = m_pFactory->CreateNode(XFA_XDPPACKET_ConnectionSet,
                                     XFA_Element::ConnectionSet);
      if (!pNode) {
        return nullptr;
      }
      pNode->SetCData(XFA_ATTRIBUTE_Name,
                      XFA_GetPacketByIndex(XFA_PACKET_ConnectionSet)->pName);
      if (!NormalLoader(pNode, pXMLDocumentNode, ePacketID)) {
        return nullptr;
      }
    }
  } else if (ePacketID == XFA_XDPPACKET_SourceSet) {
    if (XFA_FDEExtension_MatchNodeName(
            pXMLDocumentNode, XFA_GetPacketByIndex(XFA_PACKET_SourceSet)->pName,
            XFA_GetPacketByIndex(XFA_PACKET_SourceSet)->pURI,
            XFA_GetPacketByIndex(XFA_PACKET_SourceSet)->eFlags)) {
      pNode = m_pFactory->CreateNode(XFA_XDPPACKET_SourceSet,
                                     XFA_Element::SourceSet);
      if (!pNode) {
        return nullptr;
      }
      pNode->SetCData(XFA_ATTRIBUTE_Name,
                      XFA_GetPacketByIndex(XFA_PACKET_SourceSet)->pName);
      if (!NormalLoader(pNode, pXMLDocumentNode, ePacketID)) {
        return nullptr;
      }
    }
  }
  if (pNode) {
    pNode->SetXMLMappingNode(pXMLDocumentNode);
  }
  return pNode;
}
CXFA_Node* CXFA_SimpleParser::ParseAsXDPPacket_Xdc(
    CFDE_XMLNode* pXMLDocumentNode,
    XFA_XDPPACKET ePacketID) {
  if (XFA_FDEExtension_MatchNodeName(
          pXMLDocumentNode, XFA_GetPacketByIndex(XFA_PACKET_Xdc)->pName,
          XFA_GetPacketByIndex(XFA_PACKET_Xdc)->pURI,
          XFA_GetPacketByIndex(XFA_PACKET_Xdc)->eFlags)) {
    CXFA_Node* pNode =
        m_pFactory->CreateNode(XFA_XDPPACKET_Xdc, XFA_Element::Xdc);
    if (!pNode) {
      return nullptr;
    }
    pNode->SetCData(XFA_ATTRIBUTE_Name,
                    XFA_GetPacketByIndex(XFA_PACKET_Xdc)->pName);
    pNode->SetXMLMappingNode(pXMLDocumentNode);
    return pNode;
  }
  return nullptr;
}
CXFA_Node* CXFA_SimpleParser::ParseAsXDPPacket_User(
    CFDE_XMLNode* pXMLDocumentNode,
    XFA_XDPPACKET ePacketID) {
  CXFA_Node* pNode =
      m_pFactory->CreateNode(XFA_XDPPACKET_XDP, XFA_Element::Packet);
  if (!pNode) {
    return nullptr;
  }
  CFX_WideString wsName;
  static_cast<CFDE_XMLElement*>(pXMLDocumentNode)->GetLocalTagName(wsName);
  pNode->SetCData(XFA_ATTRIBUTE_Name, wsName);
  if (!UserPacketLoader(pNode, pXMLDocumentNode)) {
    return nullptr;
  }
  pNode->SetXMLMappingNode(pXMLDocumentNode);
  return pNode;
}
CXFA_Node* CXFA_SimpleParser::UserPacketLoader(CXFA_Node* pXFANode,
                                               CFDE_XMLNode* pXMLDoc) {
  return pXFANode;
}
static FX_BOOL XFA_FDEExtension_IsStringAllWhitespace(CFX_WideString wsText) {
  wsText.TrimRight(L"\x20\x9\xD\xA");
  return wsText.IsEmpty();
}
CXFA_Node* CXFA_SimpleParser::DataLoader(CXFA_Node* pXFANode,
                                         CFDE_XMLNode* pXMLDoc,
                                         FX_BOOL bDoTransform) {
  ParseDataGroup(pXFANode, pXMLDoc, XFA_XDPPACKET_Datasets);
  return pXFANode;
}
CXFA_Node* CXFA_SimpleParser::NormalLoader(CXFA_Node* pXFANode,
                                           CFDE_XMLNode* pXMLDoc,
                                           XFA_XDPPACKET ePacketID,
                                           FX_BOOL bUseAttribute) {
  FX_BOOL bOneOfPropertyFound = FALSE;
  for (CFDE_XMLNode* pXMLChild = pXMLDoc->GetNodeItem(CFDE_XMLNode::FirstChild);
       pXMLChild;
       pXMLChild = pXMLChild->GetNodeItem(CFDE_XMLNode::NextSibling)) {
    switch (pXMLChild->GetType()) {
      case FDE_XMLNODE_Element: {
        CFDE_XMLElement* pXMLElement = static_cast<CFDE_XMLElement*>(pXMLChild);
        CFX_WideString wsTagName;
        pXMLElement->GetLocalTagName(wsTagName);
        XFA_Element eType = XFA_GetElementTypeForName(wsTagName.AsStringC());
        if (eType == XFA_Element::Unknown)
          continue;

        const XFA_PROPERTY* pPropertyInfo = XFA_GetPropertyOfElement(
            pXFANode->GetElementType(), eType, ePacketID);
        if (pPropertyInfo &&
            ((pPropertyInfo->uFlags &
              (XFA_PROPERTYFLAG_OneOf | XFA_PROPERTYFLAG_DefaultOneOf)) != 0)) {
          if (bOneOfPropertyFound) {
            break;
          }
          bOneOfPropertyFound = TRUE;
        }
        CXFA_Node* pXFAChild = m_pFactory->CreateNode(ePacketID, eType);
        if (!pXFAChild)
          return nullptr;
        if (ePacketID == XFA_XDPPACKET_Config)
          pXFAChild->SetAttribute(XFA_ATTRIBUTE_Name, wsTagName.AsStringC());

        FX_BOOL IsNeedValue = TRUE;
        for (int32_t i = 0, count = pXMLElement->CountAttributes(); i < count;
             i++) {
          CFX_WideString wsAttrQualifiedName;
          CFX_WideString wsAttrName;
          CFX_WideString wsAttrValue;
          pXMLElement->GetAttribute(i, wsAttrQualifiedName, wsAttrValue);
          XFA_FDEExtension_GetAttributeLocalName(
              wsAttrQualifiedName.AsStringC(), wsAttrName);
          if (wsAttrName == FX_WSTRC(L"nil") &&
              wsAttrValue == FX_WSTRC(L"true")) {
            IsNeedValue = FALSE;
          }
          const XFA_ATTRIBUTEINFO* lpAttrInfo =
              XFA_GetAttributeByName(wsAttrName.AsStringC());
          if (!lpAttrInfo) {
            continue;
          }
          if (!bUseAttribute && lpAttrInfo->eName != XFA_ATTRIBUTE_Name &&
              lpAttrInfo->eName != XFA_ATTRIBUTE_Save) {
            continue;
          }
          pXFAChild->SetAttribute(lpAttrInfo->eName, wsAttrValue.AsStringC());
        }
        pXFANode->InsertChild(pXFAChild);
        if (eType == XFA_Element::Validate || eType == XFA_Element::Locale) {
          if (ePacketID == XFA_XDPPACKET_Config) {
            ParseContentNode(pXFAChild, pXMLElement, ePacketID);
          } else {
            NormalLoader(pXFAChild, pXMLElement, ePacketID, bUseAttribute);
          }
          break;
        }
        switch (pXFAChild->GetObjectType()) {
          case XFA_ObjectType::ContentNode:
          case XFA_ObjectType::TextNode:
          case XFA_ObjectType::NodeC:
          case XFA_ObjectType::NodeV:
            if (IsNeedValue) {
              ParseContentNode(pXFAChild, pXMLElement, ePacketID);
            }
            break;
          default:
            NormalLoader(pXFAChild, pXMLElement, ePacketID, bUseAttribute);
            break;
        }
      } break;
      case FDE_XMLNODE_Instruction:
        ParseInstruction(pXFANode, static_cast<CFDE_XMLInstruction*>(pXMLChild),
                         ePacketID);
        break;
      default:
        break;
    }
  }
  return pXFANode;
}
FX_BOOL XFA_RecognizeRichText(CFDE_XMLElement* pRichTextXMLNode) {
  if (pRichTextXMLNode) {
    CFX_WideString wsNamespaceURI;
    XFA_FDEExtension_GetElementTagNamespaceURI(pRichTextXMLNode,
                                               wsNamespaceURI);
    if (wsNamespaceURI == FX_WSTRC(L"http://www.w3.org/1999/xhtml")) {
      return TRUE;
    }
  }
  return FALSE;
}
class RichTextNodeVisitor {
 public:
  static inline CFDE_XMLNode* GetFirstChild(CFDE_XMLNode* pNode) {
    return pNode->GetNodeItem(CFDE_XMLNode::FirstChild);
  }
  static inline CFDE_XMLNode* GetNextSibling(CFDE_XMLNode* pNode) {
    return pNode->GetNodeItem(CFDE_XMLNode::NextSibling);
  }
  static inline CFDE_XMLNode* GetParent(CFDE_XMLNode* pNode) {
    return pNode->GetNodeItem(CFDE_XMLNode::Parent);
  }
};

void XFA_ConvertXMLToPlainText(CFDE_XMLElement* pRootXMLNode,
                               CFX_WideString& wsOutput) {
  for (CFDE_XMLNode* pXMLChild =
           pRootXMLNode->GetNodeItem(CFDE_XMLNode::FirstChild);
       pXMLChild;
       pXMLChild = pXMLChild->GetNodeItem(CFDE_XMLNode::NextSibling)) {
    switch (pXMLChild->GetType()) {
      case FDE_XMLNODE_Element: {
        CFX_WideString wsTextData;
        static_cast<CFDE_XMLElement*>(pXMLChild)->GetTextData(wsTextData);
        wsTextData += FX_WSTRC(L"\n");
        wsOutput += wsTextData;
      } break;
      case FDE_XMLNODE_Text: {
        CFX_WideString wsText;
        static_cast<CFDE_XMLText*>(pXMLChild)->GetText(wsText);
        if (XFA_FDEExtension_IsStringAllWhitespace(wsText)) {
          continue;
        } else {
          wsOutput = wsText;
        }
      } break;
      case FDE_XMLNODE_CharData: {
        CFX_WideString wsCharData;
        static_cast<CFDE_XMLCharData*>(pXMLChild)->GetCharData(wsCharData);
        if (XFA_FDEExtension_IsStringAllWhitespace(wsCharData)) {
          continue;
        } else {
          wsOutput = wsCharData;
        }
      } break;
      default:
        ASSERT(FALSE);
        break;
    }
  }
}

void CXFA_SimpleParser::ParseContentNode(CXFA_Node* pXFANode,
                                         CFDE_XMLNode* pXMLNode,
                                         XFA_XDPPACKET ePacketID) {
  XFA_Element element = XFA_Element::Sharptext;
  if (pXFANode->GetElementType() == XFA_Element::ExData) {
    CFX_WideStringC wsContentType =
        pXFANode->GetCData(XFA_ATTRIBUTE_ContentType);
    if (wsContentType == FX_WSTRC(L"text/html"))
      element = XFA_Element::SharpxHTML;
    else if (wsContentType == FX_WSTRC(L"text/xml"))
      element = XFA_Element::Sharpxml;
  }
  if (element == XFA_Element::SharpxHTML)
    pXFANode->SetXMLMappingNode(pXMLNode);

  CFX_WideString wsValue;
  for (CFDE_XMLNode* pXMLChild =
           pXMLNode->GetNodeItem(CFDE_XMLNode::FirstChild);
       pXMLChild;
       pXMLChild = pXMLChild->GetNodeItem(CFDE_XMLNode::NextSibling)) {
    FDE_XMLNODETYPE eNodeType = pXMLChild->GetType();
    if (eNodeType == FDE_XMLNODE_Instruction)
      continue;

    if (element == XFA_Element::SharpxHTML) {
      if (eNodeType != FDE_XMLNODE_Element)
        break;

      if (XFA_RecognizeRichText(static_cast<CFDE_XMLElement*>(pXMLChild)))
        XFA_GetPlainTextFromRichText(static_cast<CFDE_XMLElement*>(pXMLChild),
                                     wsValue);
    } else if (element == XFA_Element::Sharpxml) {
      if (eNodeType != FDE_XMLNODE_Element)
        break;
      XFA_ConvertXMLToPlainText(static_cast<CFDE_XMLElement*>(pXMLChild),
                                wsValue);
    } else {
      if (eNodeType == FDE_XMLNODE_Element)
        break;
      if (eNodeType == FDE_XMLNODE_Text)
        static_cast<CFDE_XMLText*>(pXMLChild)->GetText(wsValue);
      else if (eNodeType == FDE_XMLNODE_CharData)
        static_cast<CFDE_XMLCharData*>(pXMLChild)->GetCharData(wsValue);
    }
    break;
  }
  if (!wsValue.IsEmpty()) {
    if (pXFANode->IsContentNode()) {
      CXFA_Node* pContentRawDataNode =
          m_pFactory->CreateNode(ePacketID, element);
      ASSERT(pContentRawDataNode);
      pContentRawDataNode->SetCData(XFA_ATTRIBUTE_Value, wsValue);
      pXFANode->InsertChild(pContentRawDataNode);
    } else {
      pXFANode->SetCData(XFA_ATTRIBUTE_Value, wsValue);
    }
  }
}

void CXFA_SimpleParser::ParseDataGroup(CXFA_Node* pXFANode,
                                       CFDE_XMLNode* pXMLNode,
                                       XFA_XDPPACKET ePacketID) {
  for (CFDE_XMLNode* pXMLChild =
           pXMLNode->GetNodeItem(CFDE_XMLNode::FirstChild);
       pXMLChild;
       pXMLChild = pXMLChild->GetNodeItem(CFDE_XMLNode::NextSibling)) {
    switch (pXMLChild->GetType()) {
      case FDE_XMLNODE_Element: {
        CFDE_XMLElement* pXMLElement = static_cast<CFDE_XMLElement*>(pXMLChild);
        {
          CFX_WideString wsNamespaceURI;
          XFA_FDEExtension_GetElementTagNamespaceURI(pXMLElement,
                                                     wsNamespaceURI);
          if (wsNamespaceURI ==
                  FX_WSTRC(L"http://www.xfa.com/schema/xfa-package/") ||
              wsNamespaceURI ==
                  FX_WSTRC(L"http://www.xfa.org/schema/xfa-package/") ||
              wsNamespaceURI ==
                  FX_WSTRC(L"http://www.w3.org/2001/XMLSchema-instance")) {
            continue;
          }
          if (0) {
            continue;
          }
        }
        XFA_Element eNodeType = XFA_Element::DataModel;
        if (eNodeType == XFA_Element::DataModel) {
          CFX_WideString wsDataNodeAttr;
          if (XFA_FDEExtension_FindAttributeWithNS(
                  pXMLElement, FX_WSTRC(L"dataNode"),
                  FX_WSTRC(L"http://www.xfa.org/schema/xfa-data/1.0/"),
                  wsDataNodeAttr)) {
            if (wsDataNodeAttr == FX_WSTRC(L"dataGroup")) {
              eNodeType = XFA_Element::DataGroup;
            } else if (wsDataNodeAttr == FX_WSTRC(L"dataValue")) {
              eNodeType = XFA_Element::DataValue;
            }
          }
        }
        CFX_WideString wsContentType;
        if (eNodeType == XFA_Element::DataModel) {
          if (XFA_FDEExtension_FindAttributeWithNS(
                  pXMLElement, FX_WSTRC(L"contentType"),
                  FX_WSTRC(L"http://www.xfa.org/schema/xfa-data/1.0/"),
                  wsContentType)) {
            if (!wsContentType.IsEmpty()) {
              eNodeType = XFA_Element::DataValue;
            }
          }
        }
        if (eNodeType == XFA_Element::DataModel) {
          for (CFDE_XMLNode* pXMLDataChild =
                   pXMLElement->GetNodeItem(CFDE_XMLNode::FirstChild);
               pXMLDataChild; pXMLDataChild = pXMLDataChild->GetNodeItem(
                                  CFDE_XMLNode::NextSibling)) {
            if (pXMLDataChild->GetType() == FDE_XMLNODE_Element) {
              if (!XFA_RecognizeRichText(
                      static_cast<CFDE_XMLElement*>(pXMLDataChild))) {
                eNodeType = XFA_Element::DataGroup;
                break;
              }
            }
          }
        }
        if (eNodeType == XFA_Element::DataModel) {
          eNodeType = XFA_Element::DataValue;
        }
        CXFA_Node* pXFAChild =
            m_pFactory->CreateNode(XFA_XDPPACKET_Datasets, eNodeType);
        if (!pXFAChild) {
          return;
        }
        CFX_WideString wsNodeName;
        pXMLElement->GetLocalTagName(wsNodeName);
        pXFAChild->SetCData(XFA_ATTRIBUTE_Name, wsNodeName);
        FX_BOOL bNeedValue = TRUE;
        if (1) {
          for (int32_t i = 0, count = pXMLElement->CountAttributes(); i < count;
               i++) {
            CFX_WideString wsAttrQualifiedName;
            CFX_WideString wsAttrValue;
            CFX_WideString wsAttrName;
            CFX_WideString wsAttrNamespaceURI;
            pXMLElement->GetAttribute(i, wsAttrQualifiedName, wsAttrValue);
            if (!XFA_FDEExtension_ResolveAttribute(
                    pXMLElement, wsAttrQualifiedName.AsStringC(), wsAttrName,
                    wsAttrNamespaceURI)) {
              continue;
            }
            if (wsAttrName == FX_WSTRC(L"nil") &&
                wsAttrValue == FX_WSTRC(L"true")) {
              bNeedValue = FALSE;
              continue;
            }
            if (wsAttrNamespaceURI ==
                    FX_WSTRC(L"http://www.xfa.com/schema/xfa-package/") ||
                wsAttrNamespaceURI ==
                    FX_WSTRC(L"http://www.xfa.org/schema/xfa-package/") ||
                wsAttrNamespaceURI ==
                    FX_WSTRC(L"http://www.w3.org/2001/XMLSchema-instance") ||
                wsAttrNamespaceURI ==
                    FX_WSTRC(L"http://www.xfa.org/schema/xfa-data/1.0/")) {
              continue;
            }
            if (0) {
              continue;
            }
            CXFA_Node* pXFAMetaData = m_pFactory->CreateNode(
                XFA_XDPPACKET_Datasets, XFA_Element::DataValue);
            if (!pXFAMetaData) {
              return;
            }
            pXFAMetaData->SetCData(XFA_ATTRIBUTE_Name, wsAttrName);
            pXFAMetaData->SetCData(XFA_ATTRIBUTE_QualifiedName,
                                   wsAttrQualifiedName);
            pXFAMetaData->SetCData(XFA_ATTRIBUTE_Value, wsAttrValue);
            pXFAMetaData->SetEnum(XFA_ATTRIBUTE_Contains,
                                  XFA_ATTRIBUTEENUM_MetaData);
            pXFAChild->InsertChild(pXFAMetaData);
            pXFAMetaData->SetXMLMappingNode(pXMLElement);
            pXFAMetaData->SetFlag(XFA_NodeFlag_Initialized, false);
          }
          if (!bNeedValue) {
            CFX_WideString wsNilName(L"xsi:nil");
            pXMLElement->RemoveAttribute(wsNilName.c_str());
          }
        }
        pXFANode->InsertChild(pXFAChild);
        if (eNodeType == XFA_Element::DataGroup) {
          ParseDataGroup(pXFAChild, pXMLElement, ePacketID);
        } else {
          if (bNeedValue) {
            ParseDataValue(pXFAChild, pXMLChild, XFA_XDPPACKET_Datasets);
          }
        }
        pXFAChild->SetXMLMappingNode(pXMLElement);
        pXFAChild->SetFlag(XFA_NodeFlag_Initialized, false);
      }
        continue;
      case FDE_XMLNODE_CharData: {
        CFDE_XMLCharData* pXMLCharData =
            static_cast<CFDE_XMLCharData*>(pXMLChild);
        CFX_WideString wsCharData;
        pXMLCharData->GetCharData(wsCharData);
        if (XFA_FDEExtension_IsStringAllWhitespace(wsCharData)) {
          continue;
        }
        CXFA_Node* pXFAChild = m_pFactory->CreateNode(XFA_XDPPACKET_Datasets,
                                                      XFA_Element::DataValue);
        if (!pXFAChild) {
          return;
        }
        pXFAChild->SetCData(XFA_ATTRIBUTE_Value, wsCharData);
        pXFANode->InsertChild(pXFAChild);
        pXFAChild->SetXMLMappingNode(pXMLCharData);
        pXFAChild->SetFlag(XFA_NodeFlag_Initialized, false);
      }
        continue;
      case FDE_XMLNODE_Text: {
        CFDE_XMLText* pXMLText = static_cast<CFDE_XMLText*>(pXMLChild);
        CFX_WideString wsText;
        pXMLText->GetText(wsText);
        if (XFA_FDEExtension_IsStringAllWhitespace(wsText)) {
          continue;
        }
        CXFA_Node* pXFAChild = m_pFactory->CreateNode(XFA_XDPPACKET_Datasets,
                                                      XFA_Element::DataValue);
        if (!pXFAChild) {
          return;
        }
        pXFAChild->SetCData(XFA_ATTRIBUTE_Value, wsText);
        pXFANode->InsertChild(pXFAChild);
        pXFAChild->SetXMLMappingNode(pXMLText);
        pXFAChild->SetFlag(XFA_NodeFlag_Initialized, false);
      }
        continue;
      case FDE_XMLNODE_Instruction:
        continue;
      default:
        continue;
    }
  }
}

void CXFA_SimpleParser::ParseDataValue(CXFA_Node* pXFANode,
                                       CFDE_XMLNode* pXMLNode,
                                       XFA_XDPPACKET ePacketID) {
  CFX_WideTextBuf wsValueTextBuf;
  CFX_WideTextBuf wsCurValueTextBuf;
  FX_BOOL bMarkAsCompound = FALSE;
  CFDE_XMLNode* pXMLCurValueNode = nullptr;
  for (CFDE_XMLNode* pXMLChild =
           pXMLNode->GetNodeItem(CFDE_XMLNode::FirstChild);
       pXMLChild;
       pXMLChild = pXMLChild->GetNodeItem(CFDE_XMLNode::NextSibling)) {
    FDE_XMLNODETYPE eNodeType = pXMLChild->GetType();
    if (eNodeType == FDE_XMLNODE_Instruction)
      continue;

    CFX_WideString wsText;
    if (eNodeType == FDE_XMLNODE_Text) {
      static_cast<CFDE_XMLText*>(pXMLChild)->GetText(wsText);
      if (!pXMLCurValueNode)
        pXMLCurValueNode = pXMLChild;

      wsCurValueTextBuf << wsText;
    } else if (eNodeType == FDE_XMLNODE_CharData) {
      static_cast<CFDE_XMLCharData*>(pXMLChild)->GetCharData(wsText);
      if (!pXMLCurValueNode)
        pXMLCurValueNode = pXMLChild;

      wsCurValueTextBuf << wsText;
    } else if (XFA_RecognizeRichText(
                   static_cast<CFDE_XMLElement*>(pXMLChild))) {
      XFA_GetPlainTextFromRichText(static_cast<CFDE_XMLElement*>(pXMLChild),
                                   wsText);
      if (!pXMLCurValueNode)
        pXMLCurValueNode = pXMLChild;

      wsCurValueTextBuf << wsText;
    } else {
      bMarkAsCompound = TRUE;
      if (pXMLCurValueNode) {
        CFX_WideString wsCurValue = wsCurValueTextBuf.MakeString();
        if (!wsCurValue.IsEmpty()) {
          CXFA_Node* pXFAChild =
              m_pFactory->CreateNode(ePacketID, XFA_Element::DataValue);
          if (!pXFAChild)
            return;

          pXFAChild->SetCData(XFA_ATTRIBUTE_Name, L"");
          pXFAChild->SetCData(XFA_ATTRIBUTE_Value, wsCurValue);
          pXFANode->InsertChild(pXFAChild);
          pXFAChild->SetXMLMappingNode(pXMLCurValueNode);
          pXFAChild->SetFlag(XFA_NodeFlag_Initialized, false);
          wsValueTextBuf << wsCurValue;
          wsCurValueTextBuf.Clear();
        }
        pXMLCurValueNode = nullptr;
      }
      CXFA_Node* pXFAChild =
          m_pFactory->CreateNode(ePacketID, XFA_Element::DataValue);
      if (!pXFAChild)
        return;

      CFX_WideString wsNodeStr;
      static_cast<CFDE_XMLElement*>(pXMLChild)->GetLocalTagName(wsNodeStr);
      pXFAChild->SetCData(XFA_ATTRIBUTE_Name, wsNodeStr);
      ParseDataValue(pXFAChild, pXMLChild, ePacketID);
      pXFANode->InsertChild(pXFAChild);
      pXFAChild->SetXMLMappingNode(pXMLChild);
      pXFAChild->SetFlag(XFA_NodeFlag_Initialized, false);
      CFX_WideStringC wsCurValue = pXFAChild->GetCData(XFA_ATTRIBUTE_Value);
      wsValueTextBuf << wsCurValue;
    }
  }
  if (pXMLCurValueNode) {
    CFX_WideString wsCurValue = wsCurValueTextBuf.MakeString();
    if (!wsCurValue.IsEmpty()) {
      if (bMarkAsCompound) {
        CXFA_Node* pXFAChild =
            m_pFactory->CreateNode(ePacketID, XFA_Element::DataValue);
        if (!pXFAChild)
          return;

        pXFAChild->SetCData(XFA_ATTRIBUTE_Name, L"");
        pXFAChild->SetCData(XFA_ATTRIBUTE_Value, wsCurValue);
        pXFANode->InsertChild(pXFAChild);
        pXFAChild->SetXMLMappingNode(pXMLCurValueNode);
        pXFAChild->SetFlag(XFA_NodeFlag_Initialized, false);
      }
      wsValueTextBuf << wsCurValue;
      wsCurValueTextBuf.Clear();
    }
    pXMLCurValueNode = nullptr;
  }
  CFX_WideString wsNodeValue = wsValueTextBuf.MakeString();
  pXFANode->SetCData(XFA_ATTRIBUTE_Value, wsNodeValue);
}

void CXFA_SimpleParser::ParseInstruction(CXFA_Node* pXFANode,
                                         CFDE_XMLInstruction* pXMLInstruction,
                                         XFA_XDPPACKET ePacketID) {
  if (!m_bDocumentParser) {
    return;
  }
  CFX_WideString wsTargetName;
  pXMLInstruction->GetTargetName(wsTargetName);
  if (wsTargetName == FX_WSTRC(L"originalXFAVersion")) {
    CFX_WideString wsData;
    if (pXMLInstruction->GetData(0, wsData) &&
        (pXFANode->GetDocument()->RecognizeXFAVersionNumber(wsData) !=
         XFA_VERSION_UNKNOWN)) {
      wsData.clear();
      if (pXMLInstruction->GetData(1, wsData) &&
          wsData == FX_WSTRC(L"v2.7-scripting:1")) {
        pXFANode->GetDocument()->SetFlag(XFA_DOCFLAG_Scripting, TRUE);
      }
    }
  } else if (wsTargetName == FX_WSTRC(L"acrobat")) {
    CFX_WideString wsData;
    if (pXMLInstruction->GetData(0, wsData) &&
        wsData == FX_WSTRC(L"JavaScript")) {
      if (pXMLInstruction->GetData(1, wsData) &&
          wsData == FX_WSTRC(L"strictScoping")) {
        pXFANode->GetDocument()->SetFlag(XFA_DOCFLAG_StrictScoping, TRUE);
      }
    }
  }
}
void CXFA_SimpleParser::CloseParser() {
  if (m_pXMLDoc) {
    m_pXMLDoc->Release();
    m_pXMLDoc = nullptr;
  }
  if (m_pStream) {
    m_pStream->Release();
    m_pStream = nullptr;
  }
}

CXFA_DocumentParser::CXFA_DocumentParser(CXFA_FFNotify* pNotify)
    : m_nodeParser(nullptr, TRUE), m_pNotify(pNotify), m_pDocument(nullptr) {}
CXFA_DocumentParser::~CXFA_DocumentParser() {
  CloseParser();
}

void CXFA_DocumentParser::Release() {
  delete this;
}
int32_t CXFA_DocumentParser::StartParse(IFX_FileRead* pStream,
                                        XFA_XDPPACKET ePacketID) {
  CloseParser();
  int32_t nRetStatus = m_nodeParser.StartParse(pStream, ePacketID);
  if (nRetStatus == XFA_PARSESTATUS_Ready) {
    m_pDocument = new CXFA_Document(this);
    m_nodeParser.SetFactory(m_pDocument);
  }
  return nRetStatus;
}
int32_t CXFA_DocumentParser::DoParse(IFX_Pause* pPause) {
  int32_t nRetStatus = m_nodeParser.DoParse(pPause);
  if (nRetStatus >= XFA_PARSESTATUS_Done) {
    ASSERT(m_pDocument);
    m_pDocument->SetRoot(m_nodeParser.GetRootNode());
  }
  return nRetStatus;
}
int32_t CXFA_DocumentParser::ParseXMLData(const CFX_WideString& wsXML,
                                          CFDE_XMLNode*& pXMLNode,
                                          IFX_Pause* pPause) {
  CloseParser();
  int32_t nRetStatus = m_nodeParser.ParseXMLData(wsXML, pXMLNode, nullptr);
  if (nRetStatus == XFA_PARSESTATUS_Done && pXMLNode) {
    m_pDocument = new CXFA_Document(this);
    m_nodeParser.SetFactory(m_pDocument);
  }
  return nRetStatus;
}
void CXFA_DocumentParser::ConstructXFANode(CXFA_Node* pXFANode,
                                           CFDE_XMLNode* pXMLNode) {
  if (!pXFANode || !pXMLNode) {
    return;
  }
  m_nodeParser.ConstructXFANode(pXFANode, pXMLNode);
  CXFA_Node* pRootNode = m_nodeParser.GetRootNode();
  if (m_pDocument && pRootNode) {
    m_pDocument->SetRoot(pRootNode);
  }
}

CXFA_Document* CXFA_DocumentParser::GetFactory() const {
  return m_nodeParser.GetFactory();
}

CXFA_Node* CXFA_DocumentParser::GetRootNode() const {
  return m_nodeParser.GetRootNode();
}

CFDE_XMLDoc* CXFA_DocumentParser::GetXMLDoc() const {
  return m_nodeParser.GetXMLDoc();
}

CXFA_FFNotify* CXFA_DocumentParser::GetNotify() const {
  return m_pNotify;
}

CXFA_Document* CXFA_DocumentParser::GetDocument() const {
  return m_pDocument;
}

void CXFA_DocumentParser::CloseParser() {
  delete m_pDocument;
  m_pDocument = nullptr;
  m_nodeParser.CloseParser();
}

CXFA_XMLParser::CXFA_XMLParser(CFDE_XMLNode* pRoot, IFX_Stream* pStream)
    : m_nElementStart(0),
      m_dwCheckStatus(0),
      m_dwCurrentCheckStatus(0),
      m_pRoot(pRoot),
      m_pStream(pStream),
      m_pParser(nullptr),
      m_pParent(pRoot),
      m_pChild(nullptr),
      m_NodeStack(16),
      m_syntaxParserResult(FDE_XmlSyntaxResult::None) {
  ASSERT(m_pParent && m_pStream);
  m_NodeStack.Push(m_pParent);
  m_pParser = new CFDE_XMLSyntaxParser;
  m_pParser->Init(m_pStream, 32 * 1024, 1024 * 1024);
}

CXFA_XMLParser::~CXFA_XMLParser() {
  if (m_pParser) {
    m_pParser->Release();
  }
  m_NodeStack.RemoveAll();
  m_ws1.clear();
  m_ws2.clear();
}

void CXFA_XMLParser::Release() {
  delete this;
}

int32_t CXFA_XMLParser::DoParser(IFX_Pause* pPause) {
  if (m_syntaxParserResult == FDE_XmlSyntaxResult::Error)
    return -1;
  if (m_syntaxParserResult == FDE_XmlSyntaxResult::EndOfString)
    return 100;

  int32_t iCount = 0;
  while (TRUE) {
    m_syntaxParserResult = m_pParser->DoSyntaxParse();
    switch (m_syntaxParserResult) {
      case FDE_XmlSyntaxResult::InstructionOpen:
        break;
      case FDE_XmlSyntaxResult::InstructionClose:
        if (m_pChild) {
          if (m_pChild->GetType() != FDE_XMLNODE_Instruction) {
            m_syntaxParserResult = FDE_XmlSyntaxResult::Error;
            break;
          }
        }
        m_pChild = m_pParent;
        break;
      case FDE_XmlSyntaxResult::ElementOpen:
        if (m_dwCheckStatus != 0x03 && m_NodeStack.GetSize() == 2) {
          m_nElementStart = m_pParser->GetCurrentPos() - 1;
        }
        break;
      case FDE_XmlSyntaxResult::ElementBreak:
        break;
      case FDE_XmlSyntaxResult::ElementClose:
        if (m_pChild->GetType() != FDE_XMLNODE_Element) {
          m_syntaxParserResult = FDE_XmlSyntaxResult::Error;
          break;
        }
        m_pParser->GetTagName(m_ws1);
        static_cast<CFDE_XMLElement*>(m_pChild)->GetTagName(m_ws2);
        if (m_ws1.GetLength() > 0 && m_ws1 != m_ws2) {
          m_syntaxParserResult = FDE_XmlSyntaxResult::Error;
          break;
        }
        m_NodeStack.Pop();
        if (m_NodeStack.GetSize() < 1) {
          m_syntaxParserResult = FDE_XmlSyntaxResult::Error;
          break;
        } else if (m_dwCurrentCheckStatus != 0 && m_NodeStack.GetSize() == 2) {
          m_nSize[m_dwCurrentCheckStatus - 1] =
              m_pParser->GetCurrentBinaryPos() -
              m_nStart[m_dwCurrentCheckStatus - 1];
          m_dwCurrentCheckStatus = 0;
        }

        m_pParent = static_cast<CFDE_XMLNode*>(*m_NodeStack.GetTopElement());
        m_pChild = m_pParent;
        iCount++;
        break;
      case FDE_XmlSyntaxResult::TargetName:
        m_pParser->GetTargetName(m_ws1);
        if (m_ws1 == FX_WSTRC(L"originalXFAVersion") ||
            m_ws1 == FX_WSTRC(L"acrobat")) {
          m_pChild = new CFDE_XMLInstruction(m_ws1);
          m_pParent->InsertChildNode(m_pChild);
        } else {
          m_pChild = nullptr;
        }
        m_ws1.clear();
        break;
      case FDE_XmlSyntaxResult::TagName:
        m_pParser->GetTagName(m_ws1);
        m_pChild = new CFDE_XMLElement(m_ws1);
        m_pParent->InsertChildNode(m_pChild);
        m_NodeStack.Push(m_pChild);
        m_pParent = m_pChild;

        if (m_dwCheckStatus != 0x03 && m_NodeStack.GetSize() == 3) {
          CFX_WideString wsTag;
          static_cast<CFDE_XMLElement*>(m_pChild)->GetLocalTagName(wsTag);
          if (wsTag == FX_WSTRC(L"template")) {
            m_dwCheckStatus |= 0x01;
            m_dwCurrentCheckStatus = 0x01;
            m_nStart[0] = m_pParser->GetCurrentBinaryPos() -
                          (m_pParser->GetCurrentPos() - m_nElementStart);
          } else if (wsTag == FX_WSTRC(L"datasets")) {
            m_dwCheckStatus |= 0x02;
            m_dwCurrentCheckStatus = 0x02;
            m_nStart[1] = m_pParser->GetCurrentBinaryPos() -
                          (m_pParser->GetCurrentPos() - m_nElementStart);
          }
        }
        break;
      case FDE_XmlSyntaxResult::AttriName:
        m_pParser->GetAttributeName(m_ws1);
        break;
      case FDE_XmlSyntaxResult::AttriValue:
        if (m_pChild) {
          m_pParser->GetAttributeName(m_ws2);
          if (m_pChild->GetType() == FDE_XMLNODE_Element) {
            static_cast<CFDE_XMLElement*>(m_pChild)->SetString(m_ws1, m_ws2);
          }
        }
        m_ws1.clear();
        break;
      case FDE_XmlSyntaxResult::Text:
        m_pParser->GetTextData(m_ws1);
        m_pChild = new CFDE_XMLText(m_ws1);
        m_pParent->InsertChildNode(m_pChild);
        m_pChild = m_pParent;
        break;
      case FDE_XmlSyntaxResult::CData:
        m_pParser->GetTextData(m_ws1);
        m_pChild = new CFDE_XMLCharData(m_ws1);
        m_pParent->InsertChildNode(m_pChild);
        m_pChild = m_pParent;
        break;
      case FDE_XmlSyntaxResult::TargetData:
        if (m_pChild) {
          if (m_pChild->GetType() != FDE_XMLNODE_Instruction) {
            m_syntaxParserResult = FDE_XmlSyntaxResult::Error;
            break;
          }
          if (!m_ws1.IsEmpty()) {
            static_cast<CFDE_XMLInstruction*>(m_pChild)->AppendData(m_ws1);
          }
          m_pParser->GetTargetData(m_ws1);
          static_cast<CFDE_XMLInstruction*>(m_pChild)->AppendData(m_ws1);
        }
        m_ws1.clear();
        break;
      default:
        break;
    }
    if (m_syntaxParserResult == FDE_XmlSyntaxResult::Error ||
        m_syntaxParserResult == FDE_XmlSyntaxResult::EndOfString) {
      break;
    }
    if (pPause && iCount > 500 && pPause->NeedToPauseNow()) {
      break;
    }
  }
  return (m_syntaxParserResult == FDE_XmlSyntaxResult::Error ||
          m_NodeStack.GetSize() != 1)
             ? -1
             : m_pParser->GetStatus();
}
