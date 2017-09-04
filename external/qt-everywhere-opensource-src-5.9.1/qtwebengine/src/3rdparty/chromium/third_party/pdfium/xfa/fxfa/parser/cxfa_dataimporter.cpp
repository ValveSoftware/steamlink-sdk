// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_dataimporter.h"

#include <memory>

#include "core/fxcrt/fx_stream.h"
#include "xfa/fde/xml/fde_xml_imp.h"
#include "xfa/fxfa/fxfa.h"
#include "xfa/fxfa/fxfa_basic.h"
#include "xfa/fxfa/parser/cxfa_document.h"
#include "xfa/fxfa/parser/cxfa_simple_parser.h"
#include "xfa/fxfa/parser/xfa_object.h"

CXFA_DataImporter::CXFA_DataImporter(CXFA_Document* pDocument)
    : m_pDocument(pDocument) {
  ASSERT(m_pDocument);
}

bool CXFA_DataImporter::ImportData(IFX_SeekableReadStream* pDataDocument) {
  std::unique_ptr<CXFA_SimpleParser> pDataDocumentParser(
      new CXFA_SimpleParser(m_pDocument, false));
  if (pDataDocumentParser->StartParse(pDataDocument, XFA_XDPPACKET_Datasets) !=
      XFA_PARSESTATUS_Ready) {
    return false;
  }
  if (pDataDocumentParser->DoParse(nullptr) < XFA_PARSESTATUS_Done)
    return false;

  CXFA_Node* pImportDataRoot = pDataDocumentParser->GetRootNode();
  if (!pImportDataRoot)
    return false;

  CXFA_Node* pDataModel =
      ToNode(m_pDocument->GetXFAObject(XFA_HASHCODE_Datasets));
  if (!pDataModel)
    return false;

  CXFA_Node* pDataNode = ToNode(m_pDocument->GetXFAObject(XFA_HASHCODE_Data));
  if (pDataNode)
    pDataModel->RemoveChild(pDataNode);

  if (pImportDataRoot->GetElementType() == XFA_Element::DataModel) {
    while (CXFA_Node* pChildNode =
               pImportDataRoot->GetNodeItem(XFA_NODEITEM_FirstChild)) {
      pImportDataRoot->RemoveChild(pChildNode);
      pDataModel->InsertChild(pChildNode);
    }
  } else {
    CFDE_XMLNode* pXMLNode = pImportDataRoot->GetXMLMappingNode();
    CFDE_XMLNode* pParentXMLNode = pXMLNode->GetNodeItem(CFDE_XMLNode::Parent);
    if (pParentXMLNode)
      pParentXMLNode->RemoveChildNode(pXMLNode);
    pDataModel->InsertChild(pImportDataRoot);
  }
  m_pDocument->DoDataRemerge(false);
  return true;
}
