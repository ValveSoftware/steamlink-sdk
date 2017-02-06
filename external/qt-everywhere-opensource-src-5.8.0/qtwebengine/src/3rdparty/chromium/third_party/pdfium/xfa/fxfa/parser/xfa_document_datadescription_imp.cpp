// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/xfa_doclayout.h"
#include "xfa/fxfa/parser/xfa_document.h"
#include "xfa/fxfa/parser/xfa_localemgr.h"
#include "xfa/fxfa/parser/xfa_object.h"
#include "xfa/fxfa/parser/xfa_parser.h"
#include "xfa/fxfa/parser/xfa_script.h"
#include "xfa/fxfa/parser/xfa_utils.h"

class CXFA_TraverseStrategy_DDGroup {
 public:
  static inline CXFA_Node* GetFirstChild(CXFA_Node* pDDGroupNode) {
    return pDDGroupNode->GetFirstChildByName(XFA_HASHCODE_Group);
  }
  static inline CXFA_Node* GetNextSibling(CXFA_Node* pDDGroupNode) {
    return pDDGroupNode->GetNextSameNameSibling(XFA_HASHCODE_Group);
  }
  static inline CXFA_Node* GetParent(CXFA_Node* pDDGroupNode) {
    return pDDGroupNode->GetNodeItem(XFA_NODEITEM_Parent);
  }
};
void XFA_DataDescription_UpdateDataRelation(CXFA_Node* pDataNode,
                                            CXFA_Node* pDataDescriptionNode) {
  ASSERT(pDataDescriptionNode);
  for (CXFA_Node* pDataChild = pDataNode->GetNodeItem(XFA_NODEITEM_FirstChild);
       pDataChild;
       pDataChild = pDataChild->GetNodeItem(XFA_NODEITEM_NextSibling)) {
    uint32_t dwNameHash = pDataChild->GetNameHash();
    if (!dwNameHash)
      continue;

    CXFA_NodeIteratorTemplate<CXFA_Node, CXFA_TraverseStrategy_DDGroup>
        sIterator(pDataDescriptionNode);
    for (CXFA_Node* pDDGroupNode = sIterator.GetCurrent(); pDDGroupNode;
         pDDGroupNode = sIterator.MoveToNext()) {
      if (pDDGroupNode != pDataDescriptionNode) {
        if (pDDGroupNode->GetElementType() != XFA_Element::DataGroup)
          continue;

        CFX_WideString wsNamespace;
        if (!pDDGroupNode->TryNamespace(wsNamespace) ||
            wsNamespace != FX_WSTRC(L"http://ns.adobe.com/data-description/")) {
          continue;
        }
      }
      CXFA_Node* pDDNode = pDDGroupNode->GetFirstChildByName(dwNameHash);
      if (!pDDNode)
        continue;
      if (pDDNode->GetElementType() != pDataChild->GetElementType())
        break;

      pDataChild->SetDataDescriptionNode(pDDNode);
      XFA_DataDescription_UpdateDataRelation(pDataChild, pDDNode);
      break;
    }
  }
}
CXFA_Node* XFA_DataDescription_MaybeCreateDataNode(
    CXFA_Document* pDocument,
    CXFA_Node* pDataParent,
    XFA_Element eNodeType,
    const CFX_WideString& wsName) {
  if (!pDataParent) {
    return nullptr;
  }
  CXFA_Node* pParentDDNode = pDataParent->GetDataDescriptionNode();
  if (!pParentDDNode) {
    CXFA_Node* pDataNode =
        pDocument->CreateNode(XFA_XDPPACKET_Datasets, eNodeType);
    ASSERT(pDataNode);
    pDataNode->SetCData(XFA_ATTRIBUTE_Name, wsName);
    pDataNode->CreateXMLMappingNode();
    pDataParent->InsertChild(pDataNode);
    pDataNode->SetFlag(XFA_NodeFlag_Initialized, false);
    return pDataNode;
  } else {
    CXFA_NodeIteratorTemplate<CXFA_Node, CXFA_TraverseStrategy_DDGroup>
        sIterator(pParentDDNode);
    for (CXFA_Node* pDDGroupNode = sIterator.GetCurrent(); pDDGroupNode;
         pDDGroupNode = sIterator.MoveToNext()) {
      if (pDDGroupNode != pParentDDNode) {
        if (pDDGroupNode->GetElementType() != XFA_Element::DataGroup) {
          continue;
        }
        CFX_WideString wsNamespace;
        if (!pDDGroupNode->TryNamespace(wsNamespace) ||
            wsNamespace != FX_WSTRC(L"http://ns.adobe.com/data-description/")) {
          continue;
        }
      }
      CXFA_Node* pDDNode =
          pDDGroupNode->GetFirstChildByName(wsName.AsStringC());
      if (!pDDNode) {
        continue;
      }
      if (pDDNode->GetElementType() != eNodeType) {
        break;
      }
      CXFA_Node* pDataNode =
          pDocument->CreateNode(XFA_XDPPACKET_Datasets, eNodeType);
      ASSERT(pDataNode);
      pDataNode->SetCData(XFA_ATTRIBUTE_Name, wsName);
      pDataNode->CreateXMLMappingNode();
      if (eNodeType == XFA_Element::DataValue &&
          pDDNode->GetEnum(XFA_ATTRIBUTE_Contains) ==
              XFA_ATTRIBUTEENUM_MetaData) {
        pDataNode->SetEnum(XFA_ATTRIBUTE_Contains, XFA_ATTRIBUTEENUM_MetaData);
      }
      pDataParent->InsertChild(pDataNode);
      pDataNode->SetDataDescriptionNode(pDDNode);
      pDataNode->SetFlag(XFA_NodeFlag_Initialized, false);
      return pDataNode;
    }
    return nullptr;
  }
}
