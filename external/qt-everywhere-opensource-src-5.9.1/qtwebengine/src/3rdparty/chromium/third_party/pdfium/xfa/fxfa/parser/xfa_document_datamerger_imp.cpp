// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/xfa_document_datamerger_imp.h"

#include "core/fxcrt/fx_ext.h"
#include "xfa/fde/xml/fde_xml_imp.h"
#include "xfa/fxfa/parser/cxfa_document.h"
#include "xfa/fxfa/parser/cxfa_layoutprocessor.h"
#include "xfa/fxfa/parser/cxfa_occur.h"
#include "xfa/fxfa/parser/cxfa_scriptcontext.h"
#include "xfa/fxfa/parser/xfa_localemgr.h"
#include "xfa/fxfa/parser/xfa_object.h"
#include "xfa/fxfa/parser/xfa_resolvenode_rs.h"
#include "xfa/fxfa/parser/xfa_utils.h"

namespace {

class CXFA_TraverseStrategy_DDGroup {
 public:
  static CXFA_Node* GetFirstChild(CXFA_Node* pDDGroupNode) {
    return pDDGroupNode->GetFirstChildByName(XFA_HASHCODE_Group);
  }
  static CXFA_Node* GetNextSibling(CXFA_Node* pDDGroupNode) {
    return pDDGroupNode->GetNextSameNameSibling(XFA_HASHCODE_Group);
  }
  static CXFA_Node* GetParent(CXFA_Node* pDDGroupNode) {
    return pDDGroupNode->GetNodeItem(XFA_NODEITEM_Parent);
  }
};

struct RecurseRecord {
  CXFA_Node* pTemplateChild;
  CXFA_Node* pDataChild;
};

bool GetOccurInfo(CXFA_Node* pOccurNode,
                  int32_t& iMin,
                  int32_t& iMax,
                  int32_t& iInit) {
  if (!pOccurNode)
    return false;

  CXFA_Occur occur(pOccurNode);
  return occur.GetOccurInfo(iMin, iMax, iInit);
}

CXFA_Node* FormValueNode_CreateChild(CXFA_Node* pValueNode, XFA_Element iType) {
  CXFA_Node* pChildNode = pValueNode->GetNodeItem(XFA_NODEITEM_FirstChild);
  if (!pChildNode) {
    if (iType == XFA_Element::Unknown)
      return nullptr;
    pChildNode = pValueNode->GetProperty(0, iType);
  }
  return pChildNode;
}

void FormValueNode_MatchNoneCreateChild(CXFA_Node* pFormNode) {
  CXFA_WidgetData* pWidgetData = pFormNode->GetWidgetData();
  ASSERT(pWidgetData);
  pWidgetData->GetUIType();
}

bool FormValueNode_SetChildContent(CXFA_Node* pValueNode,
                                   const CFX_WideString& wsContent,
                                   XFA_Element iType = XFA_Element::Unknown) {
  if (!pValueNode)
    return false;

  ASSERT(pValueNode->GetPacketID() == XFA_XDPPACKET_Form);
  CXFA_Node* pChildNode = FormValueNode_CreateChild(pValueNode, iType);
  if (!pChildNode)
    return false;

  switch (pChildNode->GetObjectType()) {
    case XFA_ObjectType::ContentNode: {
      CXFA_Node* pContentRawDataNode =
          pChildNode->GetNodeItem(XFA_NODEITEM_FirstChild);
      if (!pContentRawDataNode) {
        XFA_Element element = XFA_Element::Sharptext;
        if (pChildNode->GetElementType() == XFA_Element::ExData) {
          CFX_WideString wsContentType;
          pChildNode->GetAttribute(XFA_ATTRIBUTE_ContentType, wsContentType,
                                   false);
          if (wsContentType == FX_WSTRC(L"text/html"))
            element = XFA_Element::SharpxHTML;
          else if (wsContentType == FX_WSTRC(L"text/xml"))
            element = XFA_Element::Sharpxml;
        }
        pContentRawDataNode = pChildNode->CreateSamePacketNode(element);
        pChildNode->InsertChild(pContentRawDataNode);
      }
      pContentRawDataNode->SetCData(XFA_ATTRIBUTE_Value, wsContent);
      break;
    }
    case XFA_ObjectType::NodeC:
    case XFA_ObjectType::TextNode:
    case XFA_ObjectType::NodeV: {
      pChildNode->SetCData(XFA_ATTRIBUTE_Value, wsContent);
      break;
    }
    default:
      ASSERT(false);
      break;
  }
  return true;
}

void CreateDataBinding(CXFA_Node* pFormNode,
                       CXFA_Node* pDataNode,
                       bool bDataToForm) {
  pFormNode->SetObject(XFA_ATTRIBUTE_BindingNode, pDataNode);
  pDataNode->AddBindItem(pFormNode);
  XFA_Element eType = pFormNode->GetElementType();
  if (eType != XFA_Element::Field && eType != XFA_Element::ExclGroup)
    return;

  CXFA_WidgetData* pWidgetData = pFormNode->GetWidgetData();
  ASSERT(pWidgetData);
  XFA_Element eUIType = pWidgetData->GetUIType();
  CXFA_Value defValue(pFormNode->GetProperty(0, XFA_Element::Value));
  if (!bDataToForm) {
    CFX_WideString wsValue;
    CFX_WideString wsFormattedValue;
    switch (eUIType) {
      case XFA_Element::ImageEdit: {
        CXFA_Image image = defValue.GetImage();
        CFX_WideString wsContentType;
        CFX_WideString wsHref;
        if (image) {
          image.GetContent(wsValue);
          image.GetContentType(wsContentType);
          image.GetHref(wsHref);
        }
        CFDE_XMLElement* pXMLDataElement =
            static_cast<CFDE_XMLElement*>(pDataNode->GetXMLMappingNode());
        ASSERT(pXMLDataElement);
        pWidgetData->GetFormatDataValue(wsValue, wsFormattedValue);
        pDataNode->SetAttributeValue(wsValue, wsFormattedValue);
        pDataNode->SetCData(XFA_ATTRIBUTE_ContentType, wsContentType);
        if (!wsHref.IsEmpty())
          pXMLDataElement->SetString(L"href", wsHref);

        break;
      }
      case XFA_Element::ChoiceList:
        defValue.GetChildValueContent(wsValue);
        if (pWidgetData->GetChoiceListOpen() == XFA_ATTRIBUTEENUM_MultiSelect) {
          CFX_WideStringArray wsSelTextArray;
          pWidgetData->GetSelectedItemsValue(wsSelTextArray);
          int32_t iSize = wsSelTextArray.GetSize();
          if (iSize >= 1) {
            CXFA_Node* pValue = nullptr;
            for (int32_t i = 0; i < iSize; i++) {
              pValue = pDataNode->CreateSamePacketNode(XFA_Element::DataValue);
              pValue->SetCData(XFA_ATTRIBUTE_Name, L"value");
              pValue->CreateXMLMappingNode();
              pDataNode->InsertChild(pValue);
              pValue->SetCData(XFA_ATTRIBUTE_Value, wsSelTextArray[i]);
            }
          } else {
            CFDE_XMLNode* pXMLNode = pDataNode->GetXMLMappingNode();
            ASSERT(pXMLNode->GetType() == FDE_XMLNODE_Element);
            static_cast<CFDE_XMLElement*>(pXMLNode)->SetString(L"xfa:dataNode",
                                                               L"dataGroup");
          }
        } else if (!wsValue.IsEmpty()) {
          pWidgetData->GetFormatDataValue(wsValue, wsFormattedValue);
          pDataNode->SetAttributeValue(wsValue, wsFormattedValue);
        }
        break;
      case XFA_Element::CheckButton:
        defValue.GetChildValueContent(wsValue);
        if (wsValue.IsEmpty())
          break;

        pWidgetData->GetFormatDataValue(wsValue, wsFormattedValue);
        pDataNode->SetAttributeValue(wsValue, wsFormattedValue);
        break;
      case XFA_Element::ExclGroup: {
        CXFA_Node* pChecked = nullptr;
        CXFA_Node* pChild = pFormNode->GetNodeItem(XFA_NODEITEM_FirstChild);
        for (; pChild; pChild = pChild->GetNodeItem(XFA_NODEITEM_NextSibling)) {
          if (pChild->GetElementType() != XFA_Element::Field)
            continue;

          CXFA_Node* pValue = pChild->GetChild(0, XFA_Element::Value);
          if (!pValue)
            continue;

          CXFA_Value valueChild(pValue);
          valueChild.GetChildValueContent(wsValue);
          if (wsValue.IsEmpty())
            continue;

          CXFA_Node* pItems = pChild->GetChild(0, XFA_Element::Items);
          if (!pItems)
            continue;

          CXFA_Node* pText = pItems->GetNodeItem(XFA_NODEITEM_FirstChild);
          if (!pText)
            continue;

          CFX_WideString wsContent;
          if (pText->TryContent(wsContent) && (wsContent == wsValue)) {
            pChecked = pChild;
            wsFormattedValue = wsValue;
            pDataNode->SetAttributeValue(wsValue, wsFormattedValue);
            pFormNode->SetCData(XFA_ATTRIBUTE_Value, wsContent);
            break;
          }
        }
        if (!pChecked)
          break;

        pChild = pFormNode->GetNodeItem(XFA_NODEITEM_FirstChild);
        for (; pChild; pChild = pChild->GetNodeItem(XFA_NODEITEM_NextSibling)) {
          if (pChild == pChecked)
            continue;
          if (pChild->GetElementType() != XFA_Element::Field)
            continue;

          CXFA_Node* pValue = pChild->GetProperty(0, XFA_Element::Value);
          CXFA_Node* pItems = pChild->GetChild(0, XFA_Element::Items);
          CXFA_Node* pText =
              pItems ? pItems->GetNodeItem(XFA_NODEITEM_FirstChild) : nullptr;
          if (pText)
            pText = pText->GetNodeItem(XFA_NODEITEM_NextSibling);

          CFX_WideString wsContent;
          if (pText)
            pText->TryContent(wsContent);

          FormValueNode_SetChildContent(pValue, wsContent, XFA_Element::Text);
        }
        break;
      }
      case XFA_Element::NumericEdit: {
        defValue.GetChildValueContent(wsValue);
        if (wsValue.IsEmpty())
          break;

        CFX_WideString wsOutput;
        pWidgetData->NormalizeNumStr(wsValue, wsOutput);
        wsValue = wsOutput;
        pWidgetData->GetFormatDataValue(wsValue, wsFormattedValue);
        pDataNode->SetAttributeValue(wsValue, wsFormattedValue);
        CXFA_Node* pValue = pFormNode->GetProperty(0, XFA_Element::Value);
        FormValueNode_SetChildContent(pValue, wsValue, XFA_Element::Float);
        break;
      }
      default:
        defValue.GetChildValueContent(wsValue);
        if (wsValue.IsEmpty())
          break;

        pWidgetData->GetFormatDataValue(wsValue, wsFormattedValue);
        pDataNode->SetAttributeValue(wsValue, wsFormattedValue);
        break;
    }
    return;
  }

  CFX_WideString wsXMLValue;
  pDataNode->TryContent(wsXMLValue);
  CFX_WideString wsNormalizeValue;
  pWidgetData->GetNormalizeDataValue(wsXMLValue, wsNormalizeValue);
  pDataNode->SetAttributeValue(wsNormalizeValue, wsXMLValue);
  switch (eUIType) {
    case XFA_Element::ImageEdit: {
      FormValueNode_SetChildContent(defValue.GetNode(), wsNormalizeValue,
                                    XFA_Element::Image);
      CXFA_Image image = defValue.GetImage();
      if (image) {
        CFDE_XMLElement* pXMLDataElement =
            static_cast<CFDE_XMLElement*>(pDataNode->GetXMLMappingNode());
        ASSERT(pXMLDataElement);
        CFX_WideString wsContentType;
        CFX_WideString wsHref;
        pXMLDataElement->GetString(L"xfa:contentType", wsContentType);
        if (!wsContentType.IsEmpty()) {
          pDataNode->SetCData(XFA_ATTRIBUTE_ContentType, wsContentType);
          image.SetContentType(wsContentType);
        }
        pXMLDataElement->GetString(L"href", wsHref);
        if (!wsHref.IsEmpty())
          image.SetHref(wsHref);
      }
      break;
    }
    case XFA_Element::ChoiceList:
      if (pWidgetData->GetChoiceListOpen() == XFA_ATTRIBUTEENUM_MultiSelect) {
        CXFA_NodeArray items;
        pDataNode->GetNodeList(items);
        int32_t iCounts = items.GetSize();
        if (iCounts > 0) {
          wsNormalizeValue.clear();
          CFX_WideString wsItem;
          for (int32_t i = 0; i < iCounts; i++) {
            items[i]->TryContent(wsItem);
            wsItem = (iCounts == 1) ? wsItem : wsItem + FX_WSTRC(L"\n");
            wsNormalizeValue += wsItem;
          }
          CXFA_ExData exData = defValue.GetExData();
          ASSERT(exData);
          exData.SetContentType(iCounts == 1 ? L"text/plain" : L"text/xml");
        }
        FormValueNode_SetChildContent(defValue.GetNode(), wsNormalizeValue,
                                      XFA_Element::ExData);
      } else {
        FormValueNode_SetChildContent(defValue.GetNode(), wsNormalizeValue,
                                      XFA_Element::Text);
      }
      break;
    case XFA_Element::CheckButton:
      FormValueNode_SetChildContent(defValue.GetNode(), wsNormalizeValue,
                                    XFA_Element::Text);
      break;
    case XFA_Element::ExclGroup: {
      pWidgetData->SetSelectedMemberByValue(wsNormalizeValue.AsStringC(), false,
                                            false, false);
      break;
    }
    case XFA_Element::DateTimeEdit:
      FormValueNode_SetChildContent(defValue.GetNode(), wsNormalizeValue,
                                    XFA_Element::DateTime);
      break;
    case XFA_Element::NumericEdit: {
      CFX_WideString wsPicture;
      pWidgetData->GetPictureContent(wsPicture, XFA_VALUEPICTURE_DataBind);
      if (wsPicture.IsEmpty()) {
        CFX_WideString wsOutput;
        pWidgetData->NormalizeNumStr(wsNormalizeValue, wsOutput);
        wsNormalizeValue = wsOutput;
      }
      FormValueNode_SetChildContent(defValue.GetNode(), wsNormalizeValue,
                                    XFA_Element::Float);
      break;
    }
    case XFA_Element::Barcode:
    case XFA_Element::Button:
    case XFA_Element::PasswordEdit:
    case XFA_Element::Signature:
    case XFA_Element::TextEdit:
    default:
      FormValueNode_SetChildContent(defValue.GetNode(), wsNormalizeValue,
                                    XFA_Element::Text);
      break;
  }
}

CXFA_Node* GetGlobalBinding(CXFA_Document* pDocument, uint32_t dwNameHash) {
  CXFA_Node* pNode = nullptr;
  pDocument->m_rgGlobalBinding.Lookup(dwNameHash, pNode);
  return pNode;
}

void RegisterGlobalBinding(CXFA_Document* pDocument,
                           uint32_t dwNameHash,
                           CXFA_Node* pDataNode) {
  pDocument->m_rgGlobalBinding.SetAt(dwNameHash, pDataNode);
}

CXFA_Node* ScopeMatchGlobalBinding(CXFA_Node* pDataScope,
                                   uint32_t dwNameHash,
                                   XFA_Element eMatchDataNodeType,
                                   bool bUpLevel) {
  for (CXFA_Node *pCurDataScope = pDataScope, *pLastDataScope = nullptr;
       pCurDataScope && pCurDataScope->GetPacketID() == XFA_XDPPACKET_Datasets;
       pLastDataScope = pCurDataScope,
                 pCurDataScope =
                     pCurDataScope->GetNodeItem(XFA_NODEITEM_Parent)) {
    for (CXFA_Node* pDataChild = pCurDataScope->GetFirstChildByName(dwNameHash);
         pDataChild;
         pDataChild = pDataChild->GetNextSameNameSibling(dwNameHash)) {
      if (pDataChild == pLastDataScope ||
          (eMatchDataNodeType != XFA_Element::DataModel &&
           pDataChild->GetElementType() != eMatchDataNodeType) ||
          pDataChild->HasBindItem()) {
        continue;
      }
      return pDataChild;
    }

    for (CXFA_Node* pDataChild =
             pCurDataScope->GetFirstChildByClass(XFA_Element::DataGroup);
         pDataChild; pDataChild = pDataChild->GetNextSameClassSibling(
                         XFA_Element::DataGroup)) {
      CXFA_Node* pDataNode = ScopeMatchGlobalBinding(pDataChild, dwNameHash,
                                                     eMatchDataNodeType, false);
      if (pDataNode)
        return pDataNode;
    }
    if (!bUpLevel)
      break;
  }
  return nullptr;
}

CXFA_Node* FindGlobalDataNode(CXFA_Document* pDocument,
                              CFX_WideStringC wsName,
                              CXFA_Node* pDataScope,
                              XFA_Element eMatchNodeType) {
  if (wsName.IsEmpty())
    return nullptr;

  uint32_t dwNameHash = FX_HashCode_GetW(wsName, false);
  CXFA_Node* pBounded = GetGlobalBinding(pDocument, dwNameHash);
  if (!pBounded) {
    pBounded =
        ScopeMatchGlobalBinding(pDataScope, dwNameHash, eMatchNodeType, true);
    if (pBounded)
      RegisterGlobalBinding(pDocument, dwNameHash, pBounded);
  }
  return pBounded;
}

CXFA_Node* FindOnceDataNode(CXFA_Document* pDocument,
                            CFX_WideStringC wsName,
                            CXFA_Node* pDataScope,
                            XFA_Element eMatchNodeType) {
  if (wsName.IsEmpty())
    return nullptr;

  uint32_t dwNameHash = FX_HashCode_GetW(wsName, false);
  CXFA_Node* pLastDataScope = nullptr;
  for (CXFA_Node* pCurDataScope = pDataScope;
       pCurDataScope && pCurDataScope->GetPacketID() == XFA_XDPPACKET_Datasets;
       pCurDataScope = pCurDataScope->GetNodeItem(XFA_NODEITEM_Parent)) {
    for (CXFA_Node* pDataChild = pCurDataScope->GetFirstChildByName(dwNameHash);
         pDataChild;
         pDataChild = pDataChild->GetNextSameNameSibling(dwNameHash)) {
      if (pDataChild == pLastDataScope || pDataChild->HasBindItem() ||
          (eMatchNodeType != XFA_Element::DataModel &&
           pDataChild->GetElementType() != eMatchNodeType)) {
        continue;
      }
      return pDataChild;
    }
    pLastDataScope = pCurDataScope;
  }
  return nullptr;
}

CXFA_Node* FindDataRefDataNode(CXFA_Document* pDocument,
                               CFX_WideStringC wsRef,
                               CXFA_Node* pDataScope,
                               XFA_Element eMatchNodeType,
                               CXFA_Node* pTemplateNode,
                               bool bForceBind,
                               bool bUpLevel) {
  uint32_t dFlags = XFA_RESOLVENODE_Children | XFA_RESOLVENODE_BindNew;
  if (bUpLevel || wsRef != FX_WSTRC(L"name"))
    dFlags |= (XFA_RESOLVENODE_Parent | XFA_RESOLVENODE_Siblings);

  XFA_RESOLVENODE_RS rs;
  pDocument->GetScriptContext()->ResolveObjects(pDataScope, wsRef, rs, dFlags,
                                                pTemplateNode);
  if (rs.dwFlags == XFA_RESOLVENODE_RSTYPE_CreateNodeAll ||
      rs.dwFlags == XFA_RESOLVENODE_RSTYPE_CreateNodeMidAll ||
      rs.nodes.GetSize() > 1) {
    return pDocument->GetNotBindNode(rs.nodes);
  }

  if (rs.dwFlags == XFA_RESOLVENODE_RSTYPE_CreateNodeOne) {
    CXFA_Object* pObject = (rs.nodes.GetSize() > 0) ? rs.nodes[0] : nullptr;
    CXFA_Node* pNode = ToNode(pObject);
    return (bForceBind || !pNode || !pNode->HasBindItem()) ? pNode : nullptr;
  }
  return nullptr;
}

bool NeedGenerateForm(CXFA_Node* pTemplateChild, bool bUseInstanceManager) {
  XFA_Element eType = pTemplateChild->GetElementType();
  if (eType == XFA_Element::Variables)
    return true;
  if (pTemplateChild->IsContainerNode())
    return false;
  if (eType == XFA_Element::Proto ||
      (bUseInstanceManager && eType == XFA_Element::Occur)) {
    return false;
  }
  return true;
}

CXFA_Node* CloneOrMergeInstanceManager(CXFA_Document* pDocument,
                                       CXFA_Node* pFormParent,
                                       CXFA_Node* pTemplateNode,
                                       CXFA_NodeArray& subforms) {
  CFX_WideStringC wsSubformName = pTemplateNode->GetCData(XFA_ATTRIBUTE_Name);
  CFX_WideString wsInstMgrNodeName = FX_WSTRC(L"_") + wsSubformName;
  uint32_t dwInstNameHash =
      FX_HashCode_GetW(wsInstMgrNodeName.AsStringC(), false);
  CXFA_Node* pExistingNode = XFA_DataMerge_FindFormDOMInstance(
      pDocument, XFA_Element::InstanceManager, dwInstNameHash, pFormParent);
  if (pExistingNode) {
    uint32_t dwNameHash = pTemplateNode->GetNameHash();
    for (CXFA_Node* pNode =
             pExistingNode->GetNodeItem(XFA_NODEITEM_NextSibling);
         pNode;) {
      XFA_Element eCurType = pNode->GetElementType();
      if (eCurType == XFA_Element::InstanceManager)
        break;

      if ((eCurType != XFA_Element::Subform) &&
          (eCurType != XFA_Element::SubformSet)) {
        pNode = pNode->GetNodeItem(XFA_NODEITEM_NextSibling);
        continue;
      }
      if (dwNameHash != pNode->GetNameHash())
        break;

      CXFA_Node* pNextNode = pNode->GetNodeItem(XFA_NODEITEM_NextSibling);
      pFormParent->RemoveChild(pNode);
      subforms.Add(pNode);
      pNode = pNextNode;
    }
    pFormParent->RemoveChild(pExistingNode);
    pFormParent->InsertChild(pExistingNode);
    pExistingNode->ClearFlag(XFA_NodeFlag_UnusedNode);
    pExistingNode->SetTemplateNode(pTemplateNode);
    return pExistingNode;
  }

  CXFA_Node* pNewNode =
      pDocument->CreateNode(XFA_XDPPACKET_Form, XFA_Element::InstanceManager);
  wsInstMgrNodeName =
      FX_WSTRC(L"_") + pTemplateNode->GetCData(XFA_ATTRIBUTE_Name);
  pNewNode->SetCData(XFA_ATTRIBUTE_Name, wsInstMgrNodeName);
  pFormParent->InsertChild(pNewNode, nullptr);
  pNewNode->SetTemplateNode(pTemplateNode);
  return pNewNode;
}

CXFA_Node* FindMatchingDataNode(
    CXFA_Document* pDocument,
    CXFA_Node* pTemplateNode,
    CXFA_Node* pDataScope,
    bool& bAccessedDataDOM,
    bool bForceBind,
    CXFA_NodeIteratorTemplate<CXFA_Node,
                              CXFA_TraverseStrategy_XFAContainerNode>*
        pIterator,
    bool& bSelfMatch,
    XFA_ATTRIBUTEENUM& eBindMatch,
    bool bUpLevel) {
  bool bOwnIterator = false;
  if (!pIterator) {
    bOwnIterator = true;
    pIterator = new CXFA_NodeIteratorTemplate<
        CXFA_Node, CXFA_TraverseStrategy_XFAContainerNode>(pTemplateNode);
  }

  CXFA_Node* pResult = nullptr;
  for (CXFA_Node* pCurTemplateNode = pIterator->GetCurrent();
       pCurTemplateNode;) {
    XFA_Element eMatchNodeType;
    switch (pCurTemplateNode->GetElementType()) {
      case XFA_Element::Subform:
        eMatchNodeType = XFA_Element::DataGroup;
        break;
      case XFA_Element::Field: {
        eMatchNodeType = XFA_FieldIsMultiListBox(pCurTemplateNode)
                             ? XFA_Element::DataGroup
                             : XFA_Element::DataValue;
      } break;
      case XFA_Element::ExclGroup:
        eMatchNodeType = XFA_Element::DataValue;
        break;
      default:
        pCurTemplateNode = pIterator->MoveToNext();
        continue;
    }
    CXFA_Node* pTemplateNodeOccur =
        pCurTemplateNode->GetFirstChildByClass(XFA_Element::Occur);
    int32_t iMin, iMax, iInit;
    if (pTemplateNodeOccur &&
        GetOccurInfo(pTemplateNodeOccur, iMin, iMax, iInit) && iMax == 0) {
      pCurTemplateNode = pIterator->MoveToNext();
      continue;
    }

    CXFA_Node* pTemplateNodeBind =
        pCurTemplateNode->GetFirstChildByClass(XFA_Element::Bind);
    XFA_ATTRIBUTEENUM eMatch =
        pTemplateNodeBind ? pTemplateNodeBind->GetEnum(XFA_ATTRIBUTE_Match)
                          : XFA_ATTRIBUTEENUM_Once;
    eBindMatch = eMatch;
    switch (eMatch) {
      case XFA_ATTRIBUTEENUM_None:
        pCurTemplateNode = pIterator->MoveToNext();
        continue;
      case XFA_ATTRIBUTEENUM_Global:
        bAccessedDataDOM = true;
        if (!bForceBind) {
          pCurTemplateNode = pIterator->MoveToNext();
          continue;
        }
        if (eMatchNodeType == XFA_Element::DataValue ||
            (eMatchNodeType == XFA_Element::DataGroup &&
             XFA_FieldIsMultiListBox(pTemplateNodeBind))) {
          CXFA_Node* pGlobalBindNode = FindGlobalDataNode(
              pDocument, pCurTemplateNode->GetCData(XFA_ATTRIBUTE_Name),
              pDataScope, eMatchNodeType);
          if (!pGlobalBindNode) {
            pCurTemplateNode = pIterator->MoveToNext();
            continue;
          }
          pResult = pGlobalBindNode;
          break;
        }
      case XFA_ATTRIBUTEENUM_Once: {
        bAccessedDataDOM = true;
        CXFA_Node* pOnceBindNode = FindOnceDataNode(
            pDocument, pCurTemplateNode->GetCData(XFA_ATTRIBUTE_Name),
            pDataScope, eMatchNodeType);
        if (!pOnceBindNode) {
          pCurTemplateNode = pIterator->MoveToNext();
          continue;
        }
        pResult = pOnceBindNode;
        break;
      }
      case XFA_ATTRIBUTEENUM_DataRef: {
        bAccessedDataDOM = true;
        CXFA_Node* pDataRefBindNode = FindDataRefDataNode(
            pDocument, pTemplateNodeBind->GetCData(XFA_ATTRIBUTE_Ref),
            pDataScope, eMatchNodeType, pTemplateNode, bForceBind, bUpLevel);
        if (pDataRefBindNode &&
            pDataRefBindNode->GetElementType() == eMatchNodeType) {
          pResult = pDataRefBindNode;
        }
        if (!pResult) {
          pCurTemplateNode = pIterator->SkipChildrenAndMoveToNext();
          continue;
        }
        break;
      }
      default:
        break;
    }
    if (pCurTemplateNode == pTemplateNode && pResult)
      bSelfMatch = true;
    break;
  }
  if (bOwnIterator)
    delete pIterator;
  return pResult;
}

void SortRecurseRecord(CFX_ArrayTemplate<RecurseRecord>& rgRecords,
                       CXFA_Node* pDataScope,
                       bool bChoiceMode) {
  int32_t iCount = rgRecords.GetSize();
  CFX_ArrayTemplate<RecurseRecord> rgResultRecord;
  for (CXFA_Node* pChildNode = pDataScope->GetNodeItem(XFA_NODEITEM_FirstChild);
       pChildNode;
       pChildNode = pChildNode->GetNodeItem(XFA_NODEITEM_NextSibling)) {
    for (int32_t i = 0; i < iCount; i++) {
      CXFA_Node* pNode = rgRecords[i].pDataChild;
      if (pChildNode == pNode) {
        RecurseRecord sNewRecord = {rgRecords[i].pTemplateChild, pNode};
        rgResultRecord.Add(sNewRecord);
        rgRecords.RemoveAt(i);
        iCount--;
        break;
      }
    }
    if (bChoiceMode && rgResultRecord.GetSize() > 0)
      break;
  }

  if (rgResultRecord.GetSize() > 0) {
    if (!bChoiceMode) {
      for (int32_t i = 0; i < iCount; i++) {
        RecurseRecord sNewRecord = {rgRecords[i].pTemplateChild,
                                    rgRecords[i].pDataChild};
        rgResultRecord.Add(sNewRecord);
      }
    }
    rgRecords.RemoveAll();
    rgRecords.Copy(rgResultRecord);
  }
}

CXFA_Node* CopyContainer_SubformSet(CXFA_Document* pDocument,
                                    CXFA_Node* pTemplateNode,
                                    CXFA_Node* pFormParentNode,
                                    CXFA_Node* pDataScope,
                                    bool bOneInstance,
                                    bool bDataMerge) {
  XFA_Element eType = pTemplateNode->GetElementType();
  CXFA_Node* pOccurNode = nullptr;
  CXFA_Node* pFirstInstance = nullptr;
  bool bUseInstanceManager =
      pFormParentNode->GetElementType() != XFA_Element::Area;
  CXFA_Node* pInstMgrNode = nullptr;
  CXFA_NodeArray subformArray;
  CXFA_NodeArray* pSearchArray = nullptr;
  if (!bOneInstance &&
      (eType == XFA_Element::SubformSet || eType == XFA_Element::Subform)) {
    pInstMgrNode = bUseInstanceManager ? CloneOrMergeInstanceManager(
                                             pDocument, pFormParentNode,
                                             pTemplateNode, subformArray)
                                       : nullptr;
    if (CXFA_Node* pOccurTemplateNode =
            pTemplateNode->GetFirstChildByClass(XFA_Element::Occur)) {
      pOccurNode = pInstMgrNode ? XFA_NodeMerge_CloneOrMergeContainer(
                                      pDocument, pInstMgrNode,
                                      pOccurTemplateNode, false, nullptr)
                                : pOccurTemplateNode;
    } else if (pInstMgrNode) {
      pOccurNode = pInstMgrNode->GetFirstChildByClass(XFA_Element::Occur);
      if (pOccurNode)
        pOccurNode->ClearFlag(XFA_NodeFlag_UnusedNode);
    }
    if (pInstMgrNode) {
      pInstMgrNode->SetFlag(XFA_NodeFlag_Initialized, true);
      pSearchArray = &subformArray;
      if (pFormParentNode->GetElementType() == XFA_Element::PageArea) {
        bOneInstance = true;
        if (subformArray.GetSize() < 1)
          pSearchArray = nullptr;
      } else if ((pTemplateNode->GetNameHash() == 0) &&
                 (subformArray.GetSize() < 1)) {
        pSearchArray = nullptr;
      }
    }
  }

  int32_t iMax = 1;
  int32_t iInit = 1;
  int32_t iMin = 1;
  if (!bOneInstance)
    GetOccurInfo(pOccurNode, iMin, iMax, iInit);

  XFA_ATTRIBUTEENUM eRelation =
      eType == XFA_Element::SubformSet
          ? pTemplateNode->GetEnum(XFA_ATTRIBUTE_Relation)
          : XFA_ATTRIBUTEENUM_Ordered;
  int32_t iCurRepeatIndex = 0;
  XFA_ATTRIBUTEENUM eParentBindMatch = XFA_ATTRIBUTEENUM_None;
  if (bDataMerge) {
    CXFA_NodeIteratorTemplate<CXFA_Node, CXFA_TraverseStrategy_XFAContainerNode>
        sNodeIterator(pTemplateNode);
    bool bAccessedDataDOM = false;
    if (eType == XFA_Element::SubformSet || eType == XFA_Element::Area) {
      sNodeIterator.MoveToNext();
    } else {
      CFX_MapPtrTemplate<CXFA_Node*, CXFA_Node*> subformMapArray;
      CXFA_NodeArray nodeArray;
      for (; iMax < 0 || iCurRepeatIndex < iMax; iCurRepeatIndex++) {
        bool bSelfMatch = false;
        XFA_ATTRIBUTEENUM eBindMatch = XFA_ATTRIBUTEENUM_None;
        CXFA_Node* pDataNode = FindMatchingDataNode(
            pDocument, pTemplateNode, pDataScope, bAccessedDataDOM, false,
            &sNodeIterator, bSelfMatch, eBindMatch, true);
        if (!pDataNode || sNodeIterator.GetCurrent() != pTemplateNode)
          break;

        eParentBindMatch = eBindMatch;
        CXFA_Node* pSubformNode = XFA_NodeMerge_CloneOrMergeContainer(
            pDocument, pFormParentNode, pTemplateNode, false, pSearchArray);
        if (!pFirstInstance)
          pFirstInstance = pSubformNode;

        CreateDataBinding(pSubformNode, pDataNode, true);
        ASSERT(pSubformNode);
        subformMapArray.SetAt(pSubformNode, pDataNode);
        nodeArray.Add(pSubformNode);
      }

      subformMapArray.GetStartPosition();
      for (int32_t iIndex = 0; iIndex < nodeArray.GetSize(); iIndex++) {
        CXFA_Node* pSubform = nodeArray[iIndex];
        CXFA_Node* pDataNode =
            reinterpret_cast<CXFA_Node*>(subformMapArray.GetValueAt(pSubform));
        for (CXFA_Node* pTemplateChild =
                 pTemplateNode->GetNodeItem(XFA_NODEITEM_FirstChild);
             pTemplateChild; pTemplateChild = pTemplateChild->GetNodeItem(
                                 XFA_NODEITEM_NextSibling)) {
          if (NeedGenerateForm(pTemplateChild, bUseInstanceManager)) {
            XFA_NodeMerge_CloneOrMergeContainer(pDocument, pSubform,
                                                pTemplateChild, true, nullptr);
          } else if (pTemplateChild->IsContainerNode()) {
            pDocument->DataMerge_CopyContainer(pTemplateChild, pSubform,
                                               pDataNode, false, true, false);
          }
        }
      }
      subformMapArray.RemoveAll();
    }

    for (; iMax < 0 || iCurRepeatIndex < iMax; iCurRepeatIndex++) {
      bool bSelfMatch = false;
      XFA_ATTRIBUTEENUM eBindMatch = XFA_ATTRIBUTEENUM_None;
      if (!FindMatchingDataNode(pDocument, pTemplateNode, pDataScope,
                                bAccessedDataDOM, false, &sNodeIterator,
                                bSelfMatch, eBindMatch, true)) {
        break;
      }
      if (eBindMatch == XFA_ATTRIBUTEENUM_DataRef &&
          eParentBindMatch == XFA_ATTRIBUTEENUM_DataRef) {
        break;
      }

      if (eRelation == XFA_ATTRIBUTEENUM_Choice ||
          eRelation == XFA_ATTRIBUTEENUM_Unordered) {
        CXFA_Node* pSubformSetNode = XFA_NodeMerge_CloneOrMergeContainer(
            pDocument, pFormParentNode, pTemplateNode, false, pSearchArray);
        ASSERT(pSubformSetNode);
        if (!pFirstInstance)
          pFirstInstance = pSubformSetNode;

        CFX_ArrayTemplate<RecurseRecord> rgItemMatchList;
        CFX_ArrayTemplate<CXFA_Node*> rgItemUnmatchList;
        for (CXFA_Node* pTemplateChild =
                 pTemplateNode->GetNodeItem(XFA_NODEITEM_FirstChild);
             pTemplateChild; pTemplateChild = pTemplateChild->GetNodeItem(
                                 XFA_NODEITEM_NextSibling)) {
          if (NeedGenerateForm(pTemplateChild, bUseInstanceManager)) {
            XFA_NodeMerge_CloneOrMergeContainer(pDocument, pSubformSetNode,
                                                pTemplateChild, true, nullptr);
          } else if (pTemplateChild->IsContainerNode()) {
            bSelfMatch = false;
            eBindMatch = XFA_ATTRIBUTEENUM_None;
            if (eRelation != XFA_ATTRIBUTEENUM_Ordered) {
              CXFA_Node* pDataMatch = FindMatchingDataNode(
                  pDocument, pTemplateChild, pDataScope, bAccessedDataDOM,
                  false, nullptr, bSelfMatch, eBindMatch, true);
              if (pDataMatch) {
                RecurseRecord sNewRecord = {pTemplateChild, pDataMatch};
                if (bSelfMatch)
                  rgItemMatchList.InsertAt(0, sNewRecord);
                else
                  rgItemMatchList.Add(sNewRecord);
              } else {
                rgItemUnmatchList.Add(pTemplateChild);
              }
            } else {
              rgItemUnmatchList.Add(pTemplateChild);
            }
          }
        }

        switch (eRelation) {
          case XFA_ATTRIBUTEENUM_Choice: {
            ASSERT(rgItemMatchList.GetSize());
            SortRecurseRecord(rgItemMatchList, pDataScope, true);
            pDocument->DataMerge_CopyContainer(
                rgItemMatchList[0].pTemplateChild, pSubformSetNode, pDataScope,
                false, true, true);
            break;
          }
          case XFA_ATTRIBUTEENUM_Unordered: {
            if (rgItemMatchList.GetSize()) {
              SortRecurseRecord(rgItemMatchList, pDataScope, false);
              for (int32_t i = 0, count = rgItemMatchList.GetSize(); i < count;
                   i++) {
                pDocument->DataMerge_CopyContainer(
                    rgItemMatchList[i].pTemplateChild, pSubformSetNode,
                    pDataScope, false, true, true);
              }
            }
            for (int32_t i = 0, count = rgItemUnmatchList.GetSize(); i < count;
                 i++) {
              pDocument->DataMerge_CopyContainer(rgItemUnmatchList[i],
                                                 pSubformSetNode, pDataScope,
                                                 false, true, true);
            }
            break;
          }
          default:
            break;
        }
      } else {
        CXFA_Node* pSubformSetNode = XFA_NodeMerge_CloneOrMergeContainer(
            pDocument, pFormParentNode, pTemplateNode, false, pSearchArray);
        ASSERT(pSubformSetNode);
        if (!pFirstInstance)
          pFirstInstance = pSubformSetNode;

        for (CXFA_Node* pTemplateChild =
                 pTemplateNode->GetNodeItem(XFA_NODEITEM_FirstChild);
             pTemplateChild; pTemplateChild = pTemplateChild->GetNodeItem(
                                 XFA_NODEITEM_NextSibling)) {
          if (NeedGenerateForm(pTemplateChild, bUseInstanceManager)) {
            XFA_NodeMerge_CloneOrMergeContainer(pDocument, pSubformSetNode,
                                                pTemplateChild, true, nullptr);
          } else if (pTemplateChild->IsContainerNode()) {
            pDocument->DataMerge_CopyContainer(pTemplateChild, pSubformSetNode,
                                               pDataScope, false, true, true);
          }
        }
      }
    }

    if (iCurRepeatIndex == 0 && bAccessedDataDOM == false) {
      int32_t iLimit = iMax;
      if (pInstMgrNode && pTemplateNode->GetNameHash() == 0) {
        iLimit = subformArray.GetSize();
        if (iLimit < iMin)
          iLimit = iInit;
      }

      for (; (iLimit < 0 || iCurRepeatIndex < iLimit); iCurRepeatIndex++) {
        if (pInstMgrNode) {
          if (pSearchArray && pSearchArray->GetSize() < 1) {
            if (pTemplateNode->GetNameHash() != 0)
              break;
            pSearchArray = nullptr;
          }
        } else if (!XFA_DataMerge_FindFormDOMInstance(
                       pDocument, pTemplateNode->GetElementType(),
                       pTemplateNode->GetNameHash(), pFormParentNode)) {
          break;
        }
        CXFA_Node* pSubformNode = XFA_NodeMerge_CloneOrMergeContainer(
            pDocument, pFormParentNode, pTemplateNode, false, pSearchArray);
        ASSERT(pSubformNode);
        if (!pFirstInstance)
          pFirstInstance = pSubformNode;

        for (CXFA_Node* pTemplateChild =
                 pTemplateNode->GetNodeItem(XFA_NODEITEM_FirstChild);
             pTemplateChild; pTemplateChild = pTemplateChild->GetNodeItem(
                                 XFA_NODEITEM_NextSibling)) {
          if (NeedGenerateForm(pTemplateChild, bUseInstanceManager)) {
            XFA_NodeMerge_CloneOrMergeContainer(pDocument, pSubformNode,
                                                pTemplateChild, true, nullptr);
          } else if (pTemplateChild->IsContainerNode()) {
            pDocument->DataMerge_CopyContainer(pTemplateChild, pSubformNode,
                                               pDataScope, false, true, true);
          }
        }
      }
    }
  }

  int32_t iMinimalLimit = iCurRepeatIndex == 0 ? iInit : iMin;
  for (; iCurRepeatIndex < iMinimalLimit; iCurRepeatIndex++) {
    CXFA_Node* pSubformSetNode = XFA_NodeMerge_CloneOrMergeContainer(
        pDocument, pFormParentNode, pTemplateNode, false, pSearchArray);
    ASSERT(pSubformSetNode);
    if (!pFirstInstance)
      pFirstInstance = pSubformSetNode;

    bool bFound = false;
    for (CXFA_Node* pTemplateChild =
             pTemplateNode->GetNodeItem(XFA_NODEITEM_FirstChild);
         pTemplateChild; pTemplateChild = pTemplateChild->GetNodeItem(
                             XFA_NODEITEM_NextSibling)) {
      if (NeedGenerateForm(pTemplateChild, bUseInstanceManager)) {
        XFA_NodeMerge_CloneOrMergeContainer(pDocument, pSubformSetNode,
                                            pTemplateChild, true, nullptr);
      } else if (pTemplateChild->IsContainerNode()) {
        if (bFound && eRelation == XFA_ATTRIBUTEENUM_Choice)
          continue;

        pDocument->DataMerge_CopyContainer(pTemplateChild, pSubformSetNode,
                                           pDataScope, false, bDataMerge, true);
        bFound = true;
      }
    }
  }
  return pFirstInstance;
}

CXFA_Node* CopyContainer_Field(CXFA_Document* pDocument,
                               CXFA_Node* pTemplateNode,
                               CXFA_Node* pFormNode,
                               CXFA_Node* pDataScope,
                               bool bDataMerge,
                               bool bUpLevel) {
  CXFA_Node* pFieldNode = XFA_NodeMerge_CloneOrMergeContainer(
      pDocument, pFormNode, pTemplateNode, false, nullptr);
  ASSERT(pFieldNode);
  for (CXFA_Node* pTemplateChildNode =
           pTemplateNode->GetNodeItem(XFA_NODEITEM_FirstChild);
       pTemplateChildNode; pTemplateChildNode = pTemplateChildNode->GetNodeItem(
                               XFA_NODEITEM_NextSibling)) {
    if (NeedGenerateForm(pTemplateChildNode, true)) {
      XFA_NodeMerge_CloneOrMergeContainer(pDocument, pFieldNode,
                                          pTemplateChildNode, true, nullptr);
    } else if (pTemplateNode->GetElementType() == XFA_Element::ExclGroup &&
               pTemplateChildNode->IsContainerNode()) {
      if (pTemplateChildNode->GetElementType() == XFA_Element::Field) {
        CopyContainer_Field(pDocument, pTemplateChildNode, pFieldNode, nullptr,
                            false, true);
      }
    }
  }
  if (bDataMerge) {
    bool bAccessedDataDOM = false;
    bool bSelfMatch = false;
    XFA_ATTRIBUTEENUM eBindMatch;
    CXFA_Node* pDataNode = FindMatchingDataNode(
        pDocument, pTemplateNode, pDataScope, bAccessedDataDOM, true, nullptr,
        bSelfMatch, eBindMatch, bUpLevel);
    if (pDataNode)
      CreateDataBinding(pFieldNode, pDataNode, true);
  } else {
    FormValueNode_MatchNoneCreateChild(pFieldNode);
  }
  return pFieldNode;
}

CXFA_Node* MaybeCreateDataNode(CXFA_Document* pDocument,
                               CXFA_Node* pDataParent,
                               XFA_Element eNodeType,
                               const CFX_WideString& wsName) {
  if (!pDataParent)
    return nullptr;

  CXFA_Node* pParentDDNode = pDataParent->GetDataDescriptionNode();
  if (!pParentDDNode) {
    CXFA_Node* pDataNode =
        pDocument->CreateNode(XFA_XDPPACKET_Datasets, eNodeType);
    pDataNode->SetCData(XFA_ATTRIBUTE_Name, wsName);
    pDataNode->CreateXMLMappingNode();
    pDataParent->InsertChild(pDataNode);
    pDataNode->SetFlag(XFA_NodeFlag_Initialized, false);
    return pDataNode;
  }

  CXFA_NodeIteratorTemplate<CXFA_Node, CXFA_TraverseStrategy_DDGroup> sIterator(
      pParentDDNode);
  for (CXFA_Node* pDDGroupNode = sIterator.GetCurrent(); pDDGroupNode;
       pDDGroupNode = sIterator.MoveToNext()) {
    if (pDDGroupNode != pParentDDNode) {
      if (pDDGroupNode->GetElementType() != XFA_Element::DataGroup)
        continue;

      CFX_WideString wsNamespace;
      if (!pDDGroupNode->TryNamespace(wsNamespace) ||
          wsNamespace != FX_WSTRC(L"http://ns.adobe.com/data-description/")) {
        continue;
      }
    }
    CXFA_Node* pDDNode = pDDGroupNode->GetFirstChildByName(wsName.AsStringC());
    if (!pDDNode)
      continue;
    if (pDDNode->GetElementType() != eNodeType)
      break;

    CXFA_Node* pDataNode =
        pDocument->CreateNode(XFA_XDPPACKET_Datasets, eNodeType);
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

void UpdateBindingRelations(CXFA_Document* pDocument,
                            CXFA_Node* pFormNode,
                            CXFA_Node* pDataScope,
                            bool bDataRef,
                            bool bParentDataRef) {
  bool bMatchRef = true;
  XFA_Element eType = pFormNode->GetElementType();
  CXFA_Node* pDataNode = pFormNode->GetBindData();
  if (eType == XFA_Element::Subform || eType == XFA_Element::ExclGroup ||
      eType == XFA_Element::Field) {
    CXFA_Node* pTemplateNode = pFormNode->GetTemplateNode();
    CXFA_Node* pTemplateNodeBind =
        pTemplateNode ? pTemplateNode->GetFirstChildByClass(XFA_Element::Bind)
                      : nullptr;
    XFA_ATTRIBUTEENUM eMatch =
        pTemplateNodeBind ? pTemplateNodeBind->GetEnum(XFA_ATTRIBUTE_Match)
                          : XFA_ATTRIBUTEENUM_Once;
    switch (eMatch) {
      case XFA_ATTRIBUTEENUM_None:
        if (!bDataRef || bParentDataRef)
          FormValueNode_MatchNoneCreateChild(pFormNode);
        break;
      case XFA_ATTRIBUTEENUM_Once:
        if (!bDataRef || bParentDataRef) {
          if (!pDataNode) {
            if (pFormNode->GetNameHash() != 0 &&
                pFormNode->GetEnum(XFA_ATTRIBUTE_Scope) !=
                    XFA_ATTRIBUTEENUM_None) {
              XFA_Element eDataNodeType = (eType == XFA_Element::Subform ||
                                           XFA_FieldIsMultiListBox(pFormNode))
                                              ? XFA_Element::DataGroup
                                              : XFA_Element::DataValue;
              pDataNode = MaybeCreateDataNode(
                  pDocument, pDataScope, eDataNodeType,
                  CFX_WideString(pFormNode->GetCData(XFA_ATTRIBUTE_Name)));
              if (pDataNode)
                CreateDataBinding(pFormNode, pDataNode, false);
            }
            if (!pDataNode)
              FormValueNode_MatchNoneCreateChild(pFormNode);

          } else {
            CXFA_Node* pDataParent =
                pDataNode->GetNodeItem(XFA_NODEITEM_Parent);
            if (pDataParent != pDataScope) {
              ASSERT(pDataParent);
              pDataParent->RemoveChild(pDataNode);
              pDataScope->InsertChild(pDataNode);
            }
          }
        }
        break;
      case XFA_ATTRIBUTEENUM_Global:
        if (!bDataRef || bParentDataRef) {
          uint32_t dwNameHash = pFormNode->GetNameHash();
          if (dwNameHash != 0 && !pDataNode) {
            pDataNode = GetGlobalBinding(pDocument, dwNameHash);
            if (!pDataNode) {
              XFA_Element eDataNodeType = (eType == XFA_Element::Subform ||
                                           XFA_FieldIsMultiListBox(pFormNode))
                                              ? XFA_Element::DataGroup
                                              : XFA_Element::DataValue;
              CXFA_Node* pRecordNode =
                  ToNode(pDocument->GetXFAObject(XFA_HASHCODE_Record));
              pDataNode = MaybeCreateDataNode(
                  pDocument, pRecordNode, eDataNodeType,
                  CFX_WideString(pFormNode->GetCData(XFA_ATTRIBUTE_Name)));
              if (pDataNode) {
                CreateDataBinding(pFormNode, pDataNode, false);
                RegisterGlobalBinding(pDocument, pFormNode->GetNameHash(),
                                      pDataNode);
              }
            } else {
              CreateDataBinding(pFormNode, pDataNode, true);
            }
          }
          if (!pDataNode)
            FormValueNode_MatchNoneCreateChild(pFormNode);
        }
        break;
      case XFA_ATTRIBUTEENUM_DataRef: {
        bMatchRef = bDataRef;
        bParentDataRef = true;
        if (!pDataNode && bDataRef) {
          CFX_WideStringC wsRef =
              pTemplateNodeBind->GetCData(XFA_ATTRIBUTE_Ref);
          uint32_t dFlags =
              XFA_RESOLVENODE_Children | XFA_RESOLVENODE_CreateNode;
          XFA_RESOLVENODE_RS rs;
          pDocument->GetScriptContext()->ResolveObjects(pDataScope, wsRef, rs,
                                                        dFlags, pTemplateNode);
          CXFA_Object* pObject =
              (rs.nodes.GetSize() > 0) ? rs.nodes[0] : nullptr;
          pDataNode = ToNode(pObject);
          if (pDataNode) {
            CreateDataBinding(pFormNode, pDataNode,
                              rs.dwFlags == XFA_RESOVENODE_RSTYPE_ExistNodes);
          } else {
            FormValueNode_MatchNoneCreateChild(pFormNode);
          }
        }
        break;
      }
      default:
        break;
    }
  }

  if (bMatchRef &&
      (eType == XFA_Element::Subform || eType == XFA_Element::SubformSet ||
       eType == XFA_Element::Area || eType == XFA_Element::PageArea ||
       eType == XFA_Element::PageSet)) {
    for (CXFA_Node* pFormChild =
             pFormNode->GetNodeItem(XFA_NODEITEM_FirstChild);
         pFormChild;
         pFormChild = pFormChild->GetNodeItem(XFA_NODEITEM_NextSibling)) {
      if (!pFormChild->IsContainerNode())
        continue;
      if (pFormChild->IsUnusedNode())
        continue;

      UpdateBindingRelations(pDocument, pFormChild,
                             pDataNode ? pDataNode : pDataScope, bDataRef,
                             bParentDataRef);
    }
  }
}

void UpdateDataRelation(CXFA_Node* pDataNode, CXFA_Node* pDataDescriptionNode) {
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
      UpdateDataRelation(pDataChild, pDDNode);
      break;
    }
  }
}

}  // namespace

CXFA_Node* XFA_DataMerge_FindFormDOMInstance(CXFA_Document* pDocument,
                                             XFA_Element eType,
                                             uint32_t dwNameHash,
                                             CXFA_Node* pFormParent) {
  CXFA_Node* pFormChild = pFormParent->GetNodeItem(XFA_NODEITEM_FirstChild);
  for (; pFormChild;
       pFormChild = pFormChild->GetNodeItem(XFA_NODEITEM_NextSibling)) {
    if (pFormChild->GetElementType() == eType &&
        pFormChild->GetNameHash() == dwNameHash && pFormChild->IsUnusedNode()) {
      return pFormChild;
    }
  }
  return nullptr;
}

CXFA_Node* XFA_NodeMerge_CloneOrMergeContainer(CXFA_Document* pDocument,
                                               CXFA_Node* pFormParent,
                                               CXFA_Node* pTemplateNode,
                                               bool bRecursive,
                                               CXFA_NodeArray* pSubformArray) {
  CXFA_Node* pExistingNode = nullptr;
  if (!pSubformArray) {
    pExistingNode = XFA_DataMerge_FindFormDOMInstance(
        pDocument, pTemplateNode->GetElementType(),
        pTemplateNode->GetNameHash(), pFormParent);
  } else if (pSubformArray->GetSize() > 0) {
    pExistingNode = pSubformArray->GetAt(0);
    pSubformArray->RemoveAt(0);
  }

  if (pExistingNode) {
    if (pSubformArray) {
      pFormParent->InsertChild(pExistingNode);
    } else if (pExistingNode->IsContainerNode()) {
      pFormParent->RemoveChild(pExistingNode);
      pFormParent->InsertChild(pExistingNode);
    }
    pExistingNode->ClearFlag(XFA_NodeFlag_UnusedNode);
    pExistingNode->SetTemplateNode(pTemplateNode);
    if (bRecursive && pExistingNode->GetElementType() != XFA_Element::Items) {
      for (CXFA_Node* pTemplateChild =
               pTemplateNode->GetNodeItem(XFA_NODEITEM_FirstChild);
           pTemplateChild; pTemplateChild = pTemplateChild->GetNodeItem(
                               XFA_NODEITEM_NextSibling)) {
        if (NeedGenerateForm(pTemplateChild, true)) {
          XFA_NodeMerge_CloneOrMergeContainer(
              pDocument, pExistingNode, pTemplateChild, bRecursive, nullptr);
        }
      }
    }
    pExistingNode->SetFlag(XFA_NodeFlag_Initialized, true);
    return pExistingNode;
  }

  CXFA_Node* pNewNode = pTemplateNode->CloneTemplateToForm(false);
  pFormParent->InsertChild(pNewNode, nullptr);
  if (bRecursive) {
    for (CXFA_Node* pTemplateChild =
             pTemplateNode->GetNodeItem(XFA_NODEITEM_FirstChild);
         pTemplateChild; pTemplateChild = pTemplateChild->GetNodeItem(
                             XFA_NODEITEM_NextSibling)) {
      if (NeedGenerateForm(pTemplateChild, true)) {
        CXFA_Node* pNewChild = pTemplateChild->CloneTemplateToForm(true);
        pNewNode->InsertChild(pNewChild, nullptr);
      }
    }
  }
  return pNewNode;
}

CXFA_Node* XFA_DataMerge_FindDataScope(CXFA_Node* pParentFormNode) {
  for (CXFA_Node* pRootBoundNode = pParentFormNode;
       pRootBoundNode && pRootBoundNode->IsContainerNode();
       pRootBoundNode = pRootBoundNode->GetNodeItem(XFA_NODEITEM_Parent)) {
    CXFA_Node* pDataScope = pRootBoundNode->GetBindData();
    if (pDataScope)
      return pDataScope;
  }
  return ToNode(
      pParentFormNode->GetDocument()->GetXFAObject(XFA_HASHCODE_Data));
}

CXFA_Node* CXFA_Document::DataMerge_CopyContainer(CXFA_Node* pTemplateNode,
                                                  CXFA_Node* pFormNode,
                                                  CXFA_Node* pDataScope,
                                                  bool bOneInstance,
                                                  bool bDataMerge,
                                                  bool bUpLevel) {
  switch (pTemplateNode->GetElementType()) {
    case XFA_Element::SubformSet:
    case XFA_Element::Subform:
    case XFA_Element::Area:
    case XFA_Element::PageArea:
      return CopyContainer_SubformSet(this, pTemplateNode, pFormNode,
                                      pDataScope, bOneInstance, bDataMerge);
    case XFA_Element::ExclGroup:
    case XFA_Element::Field:
    case XFA_Element::Draw:
    case XFA_Element::ContentArea:
      return CopyContainer_Field(this, pTemplateNode, pFormNode, pDataScope,
                                 bDataMerge, bUpLevel);
    case XFA_Element::PageSet:
    case XFA_Element::Variables:
      break;
    default:
      ASSERT(false);
      break;
  }
  return nullptr;
}

void CXFA_Document::DataMerge_UpdateBindingRelations(
    CXFA_Node* pFormUpdateRoot) {
  CXFA_Node* pDataScope = XFA_DataMerge_FindDataScope(
      pFormUpdateRoot->GetNodeItem(XFA_NODEITEM_Parent));
  if (!pDataScope)
    return;

  UpdateBindingRelations(this, pFormUpdateRoot, pDataScope, false, false);
  UpdateBindingRelations(this, pFormUpdateRoot, pDataScope, true, false);
}

CXFA_Node* CXFA_Document::GetNotBindNode(CXFA_ObjArray& arrayNodes) {
  for (int32_t i = 0; i < arrayNodes.GetSize(); i++) {
    CXFA_Node* pNode = arrayNodes[i]->AsNode();
    if (pNode && !pNode->HasBindItem())
      return pNode;
  }
  return nullptr;
}

void CXFA_Document::DoDataMerge() {
  CXFA_Node* pDatasetsRoot = ToNode(GetXFAObject(XFA_HASHCODE_Datasets));
  if (!pDatasetsRoot) {
    CFDE_XMLElement* pDatasetsXMLNode = new CFDE_XMLElement(L"xfa:datasets");
    pDatasetsXMLNode->SetString(L"xmlns:xfa",
                                L"http://www.xfa.org/schema/xfa-data/1.0/");
    pDatasetsRoot = CreateNode(XFA_XDPPACKET_Datasets, XFA_Element::DataModel);
    pDatasetsRoot->SetCData(XFA_ATTRIBUTE_Name, L"datasets");
    m_pRootNode->GetXMLMappingNode()->InsertChildNode(pDatasetsXMLNode);
    m_pRootNode->InsertChild(pDatasetsRoot);
    pDatasetsRoot->SetXMLMappingNode(pDatasetsXMLNode);
  }
  CXFA_Node *pDataRoot = nullptr, *pDDRoot = nullptr;
  CFX_WideString wsDatasetsURI;
  pDatasetsRoot->TryNamespace(wsDatasetsURI);
  for (CXFA_Node* pChildNode =
           pDatasetsRoot->GetNodeItem(XFA_NODEITEM_FirstChild);
       pChildNode;
       pChildNode = pChildNode->GetNodeItem(XFA_NODEITEM_NextSibling)) {
    if (pChildNode->GetElementType() != XFA_Element::DataGroup)
      continue;

    CFX_WideString wsNamespaceURI;
    if (!pDDRoot && pChildNode->GetNameHash() == XFA_HASHCODE_DataDescription) {
      if (!pChildNode->TryNamespace(wsNamespaceURI))
        continue;
      if (wsNamespaceURI == FX_WSTRC(L"http://ns.adobe.com/data-description/"))
        pDDRoot = pChildNode;
    } else if (!pDataRoot && pChildNode->GetNameHash() == XFA_HASHCODE_Data) {
      if (!pChildNode->TryNamespace(wsNamespaceURI))
        continue;
      if (wsNamespaceURI == wsDatasetsURI)
        pDataRoot = pChildNode;
    }
    if (pDataRoot && pDDRoot)
      break;
  }

  if (!pDataRoot) {
    CFDE_XMLElement* pDataRootXMLNode = new CFDE_XMLElement(L"xfa:data");
    pDataRoot = CreateNode(XFA_XDPPACKET_Datasets, XFA_Element::DataGroup);
    pDataRoot->SetCData(XFA_ATTRIBUTE_Name, L"data");
    pDataRoot->SetXMLMappingNode(pDataRootXMLNode);
    pDatasetsRoot->InsertChild(pDataRoot);
  }

  CXFA_Node* pDataTopLevel =
      pDataRoot->GetFirstChildByClass(XFA_Element::DataGroup);
  uint32_t dwNameHash = pDataTopLevel ? pDataTopLevel->GetNameHash() : 0;
  CXFA_Node* pTemplateRoot =
      m_pRootNode->GetFirstChildByClass(XFA_Element::Template);
  if (!pTemplateRoot)
    return;

  CXFA_Node* pTemplateChosen =
      dwNameHash != 0 ? pTemplateRoot->GetFirstChildByName(dwNameHash)
                      : nullptr;
  if (!pTemplateChosen ||
      pTemplateChosen->GetElementType() != XFA_Element::Subform) {
    pTemplateChosen = pTemplateRoot->GetFirstChildByClass(XFA_Element::Subform);
  }
  if (!pTemplateChosen)
    return;

  CXFA_Node* pFormRoot = m_pRootNode->GetFirstChildByClass(XFA_Element::Form);
  bool bEmptyForm = false;
  if (!pFormRoot) {
    bEmptyForm = true;
    pFormRoot = CreateNode(XFA_XDPPACKET_Form, XFA_Element::Form);
    ASSERT(pFormRoot);
    pFormRoot->SetCData(XFA_ATTRIBUTE_Name, L"form");
    m_pRootNode->InsertChild(pFormRoot, nullptr);
  } else {
    CXFA_NodeIteratorTemplate<CXFA_Node, CXFA_TraverseStrategy_XFANode>
        sIterator(pFormRoot);
    for (CXFA_Node* pNode = sIterator.MoveToNext(); pNode;
         pNode = sIterator.MoveToNext()) {
      pNode->SetFlag(XFA_NodeFlag_UnusedNode, true);
    }
  }

  CXFA_Node* pSubformSetNode = XFA_NodeMerge_CloneOrMergeContainer(
      this, pFormRoot, pTemplateChosen, false, nullptr);
  ASSERT(pSubformSetNode);
  if (!pDataTopLevel) {
    CFX_WideStringC wsFormName = pSubformSetNode->GetCData(XFA_ATTRIBUTE_Name);
    CFX_WideString wsDataTopLevelName(wsFormName.IsEmpty() ? L"form"
                                                           : wsFormName);
    CFDE_XMLElement* pDataTopLevelXMLNode =
        new CFDE_XMLElement(wsDataTopLevelName);

    pDataTopLevel = CreateNode(XFA_XDPPACKET_Datasets, XFA_Element::DataGroup);
    pDataTopLevel->SetCData(XFA_ATTRIBUTE_Name, wsDataTopLevelName);
    pDataTopLevel->SetXMLMappingNode(pDataTopLevelXMLNode);
    CXFA_Node* pBeforeNode = pDataRoot->GetNodeItem(XFA_NODEITEM_FirstChild);
    pDataRoot->InsertChild(pDataTopLevel, pBeforeNode);
  }

  ASSERT(pDataTopLevel);
  CreateDataBinding(pSubformSetNode, pDataTopLevel, true);
  for (CXFA_Node* pTemplateChild =
           pTemplateChosen->GetNodeItem(XFA_NODEITEM_FirstChild);
       pTemplateChild;
       pTemplateChild = pTemplateChild->GetNodeItem(XFA_NODEITEM_NextSibling)) {
    if (NeedGenerateForm(pTemplateChild, true)) {
      XFA_NodeMerge_CloneOrMergeContainer(this, pSubformSetNode, pTemplateChild,
                                          true, nullptr);
    } else if (pTemplateChild->IsContainerNode()) {
      DataMerge_CopyContainer(pTemplateChild, pSubformSetNode, pDataTopLevel,
                              false, true, true);
    }
  }
  if (pDDRoot)
    UpdateDataRelation(pDataRoot, pDDRoot);

  DataMerge_UpdateBindingRelations(pSubformSetNode);
  CXFA_Node* pPageSetNode =
      pSubformSetNode->GetFirstChildByClass(XFA_Element::PageSet);
  while (pPageSetNode) {
    m_pPendingPageSet.Add(pPageSetNode);
    CXFA_Node* pNextPageSetNode =
        pPageSetNode->GetNextSameClassSibling(XFA_Element::PageSet);
    pSubformSetNode->RemoveChild(pPageSetNode);
    pPageSetNode = pNextPageSetNode;
  }

  if (bEmptyForm)
    return;

  CXFA_NodeIteratorTemplate<CXFA_Node, CXFA_TraverseStrategy_XFANode> sIterator(
      pFormRoot);
  CXFA_Node* pNode = sIterator.MoveToNext();
  while (pNode) {
    if (pNode->IsUnusedNode()) {
      if (pNode->IsContainerNode() ||
          pNode->GetElementType() == XFA_Element::InstanceManager) {
        CXFA_Node* pNext = sIterator.SkipChildrenAndMoveToNext();
        pNode->GetNodeItem(XFA_NODEITEM_Parent)->RemoveChild(pNode);
        pNode = pNext;
      } else {
        pNode->ClearFlag(XFA_NodeFlag_UnusedNode);
        pNode->SetFlag(XFA_NodeFlag_Initialized, true);
        pNode = sIterator.MoveToNext();
      }
    } else {
      pNode->SetFlag(XFA_NodeFlag_Initialized, true);
      pNode = sIterator.MoveToNext();
    }
  }
}

void CXFA_Document::DoDataRemerge(bool bDoDataMerge) {
  CXFA_Node* pFormRoot = ToNode(GetXFAObject(XFA_HASHCODE_Form));
  if (pFormRoot) {
    while (CXFA_Node* pNode = pFormRoot->GetNodeItem(XFA_NODEITEM_FirstChild))
      pFormRoot->RemoveChild(pNode);
    pFormRoot->SetObject(XFA_ATTRIBUTE_BindingNode, nullptr);
  }
  m_rgGlobalBinding.RemoveAll();
  if (bDoDataMerge)
    DoDataMerge();

  CXFA_LayoutProcessor* pLayoutProcessor = GetLayoutProcessor();
  pLayoutProcessor->SetForceReLayout(true);
}
