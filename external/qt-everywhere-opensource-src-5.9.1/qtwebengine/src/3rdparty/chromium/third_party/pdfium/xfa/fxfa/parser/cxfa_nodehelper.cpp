// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_nodehelper.h"

#include "core/fxcrt/fx_ext.h"
#include "xfa/fxfa/parser/cxfa_document.h"
#include "xfa/fxfa/parser/cxfa_scriptcontext.h"
#include "xfa/fxfa/parser/xfa_localemgr.h"
#include "xfa/fxfa/parser/xfa_object.h"
#include "xfa/fxfa/parser/xfa_resolvenode_rs.h"
#include "xfa/fxfa/parser/xfa_utils.h"

CXFA_NodeHelper::CXFA_NodeHelper()
    : m_eLastCreateType(XFA_Element::DataValue),
      m_pCreateParent(nullptr),
      m_iCreateCount(0),
      m_iCreateFlag(XFA_RESOLVENODE_RSTYPE_CreateNodeOne),
      m_iCurAllStart(-1),
      m_pAllStartParent(nullptr) {}

CXFA_NodeHelper::~CXFA_NodeHelper() {}

CXFA_Node* CXFA_NodeHelper::ResolveNodes_GetOneChild(CXFA_Node* parent,
                                                     const FX_WCHAR* pwsName,
                                                     bool bIsClassName) {
  if (!parent) {
    return nullptr;
  }
  CXFA_NodeArray siblings;
  uint32_t uNameHash = FX_HashCode_GetW(CFX_WideStringC(pwsName), false);
  NodeAcc_TraverseAnySiblings(parent, uNameHash, &siblings, bIsClassName);
  if (siblings.GetSize() == 0) {
    return nullptr;
  }
  return siblings[0];
}

int32_t CXFA_NodeHelper::CountSiblings(CXFA_Node* pNode,
                                       XFA_LOGIC_TYPE eLogicType,
                                       CXFA_NodeArray* pSiblings,
                                       bool bIsClassName) {
  if (!pNode)
    return 0;
  CXFA_Node* parent = ResolveNodes_GetParent(pNode, XFA_LOGIC_NoTransparent);
  if (!parent)
    return 0;
  const XFA_PROPERTY* pProperty = XFA_GetPropertyOfElement(
      parent->GetElementType(), pNode->GetElementType(), XFA_XDPPACKET_UNKNOWN);
  if (!pProperty && eLogicType == XFA_LOGIC_Transparent) {
    parent = ResolveNodes_GetParent(pNode, XFA_LOGIC_Transparent);
    if (!parent) {
      return 0;
    }
  }
  if (bIsClassName) {
    return NodeAcc_TraverseSiblings(parent, pNode->GetClassHashCode(),
                                    pSiblings, eLogicType, bIsClassName);
  } else {
    return NodeAcc_TraverseSiblings(parent, pNode->GetNameHash(), pSiblings,
                                    eLogicType, bIsClassName);
  }
}

int32_t CXFA_NodeHelper::NodeAcc_TraverseAnySiblings(CXFA_Node* parent,
                                                     uint32_t dNameHash,
                                                     CXFA_NodeArray* pSiblings,
                                                     bool bIsClassName) {
  if (!parent || !pSiblings) {
    return 0;
  }
  int32_t nCount = 0;
  int32_t i = 0;
  CXFA_NodeArray properties;
  parent->GetNodeList(properties, XFA_NODEFILTER_Properties);
  int32_t nProperties = properties.GetSize();
  for (i = 0; i < nProperties; ++i) {
    CXFA_Node* child = properties[i];
    if (bIsClassName) {
      if (child->GetClassHashCode() == dNameHash) {
        pSiblings->Add(child);
        nCount++;
      }
    } else {
      if (child->GetNameHash() == dNameHash) {
        pSiblings->Add(child);
        nCount++;
      }
    }
    if (nCount > 0) {
      return nCount;
    }
    nCount +=
        NodeAcc_TraverseAnySiblings(child, dNameHash, pSiblings, bIsClassName);
  }
  CXFA_NodeArray children;
  parent->GetNodeList(children, XFA_NODEFILTER_Children);
  int32_t nChildren = children.GetSize();
  for (i = 0; i < nChildren; i++) {
    CXFA_Node* child = children[i];
    if (bIsClassName) {
      if (child->GetClassHashCode() == dNameHash) {
        if (pSiblings) {
          pSiblings->Add(child);
        }
        nCount++;
      }
    } else {
      if (child->GetNameHash() == dNameHash) {
        if (pSiblings) {
          pSiblings->Add(child);
        }
        nCount++;
      }
    }
    if (nCount > 0) {
      return nCount;
    }
    nCount +=
        NodeAcc_TraverseAnySiblings(child, dNameHash, pSiblings, bIsClassName);
  }
  return nCount;
}

int32_t CXFA_NodeHelper::NodeAcc_TraverseSiblings(CXFA_Node* parent,
                                                  uint32_t dNameHash,
                                                  CXFA_NodeArray* pSiblings,
                                                  XFA_LOGIC_TYPE eLogicType,
                                                  bool bIsClassName,
                                                  bool bIsFindProperty) {
  if (!parent || !pSiblings) {
    return 0;
  }
  int32_t nCount = 0;
  int32_t i = 0;
  if (bIsFindProperty) {
    CXFA_NodeArray properties;
    parent->GetNodeList(properties, XFA_NODEFILTER_Properties);
    int32_t nProperties = properties.GetSize();
    for (i = 0; i < nProperties; ++i) {
      CXFA_Node* child = properties[i];
      if (bIsClassName) {
        if (child->GetClassHashCode() == dNameHash) {
          pSiblings->Add(child);
          nCount++;
        }
      } else {
        if (child->GetNameHash() == dNameHash) {
          if (child->GetElementType() != XFA_Element::PageSet &&
              child->GetElementType() != XFA_Element::Extras &&
              child->GetElementType() != XFA_Element::Items) {
            pSiblings->Add(child);
            nCount++;
          }
        }
      }
      if (child->IsUnnamed() &&
          child->GetElementType() == XFA_Element::PageSet) {
        nCount += NodeAcc_TraverseSiblings(child, dNameHash, pSiblings,
                                           eLogicType, bIsClassName, false);
      }
    }
    if (nCount > 0) {
      return nCount;
    }
  }
  CXFA_NodeArray children;
  parent->GetNodeList(children, XFA_NODEFILTER_Children);
  int32_t nChildren = children.GetSize();
  for (i = 0; i < nChildren; i++) {
    CXFA_Node* child = children[i];
    if (child->GetElementType() == XFA_Element::Variables) {
      continue;
    }
    if (bIsClassName) {
      if (child->GetClassHashCode() == dNameHash) {
        if (pSiblings) {
          pSiblings->Add(child);
        }
        nCount++;
      }
    } else {
      if (child->GetNameHash() == dNameHash) {
        if (pSiblings) {
          pSiblings->Add(child);
        }
        nCount++;
      }
    }
    if (eLogicType == XFA_LOGIC_NoTransparent) {
      continue;
    }
    if (NodeIsTransparent(child) &&
        child->GetElementType() != XFA_Element::PageSet) {
      nCount += NodeAcc_TraverseSiblings(child, dNameHash, pSiblings,
                                         eLogicType, bIsClassName, false);
    }
  }
  return nCount;
}

CXFA_Node* CXFA_NodeHelper::ResolveNodes_GetParent(CXFA_Node* pNode,
                                                   XFA_LOGIC_TYPE eLogicType) {
  if (!pNode) {
    return nullptr;
  }
  if (eLogicType == XFA_LOGIC_NoTransparent) {
    return pNode->GetNodeItem(XFA_NODEITEM_Parent);
  }
  CXFA_Node* parent;
  CXFA_Node* node = pNode;
  while (true) {
    parent = ResolveNodes_GetParent(node);
    if (!parent) {
      break;
    }
    XFA_Element parentType = parent->GetElementType();
    if ((!parent->IsUnnamed() && parentType != XFA_Element::SubformSet) ||
        parentType == XFA_Element::Variables) {
      break;
    }
    node = parent;
  }
  return parent;
}

int32_t CXFA_NodeHelper::GetIndex(CXFA_Node* pNode,
                                  XFA_LOGIC_TYPE eLogicType,
                                  bool bIsProperty,
                                  bool bIsClassIndex) {
  CXFA_Node* parent = ResolveNodes_GetParent(pNode, XFA_LOGIC_NoTransparent);
  if (!parent) {
    return 0;
  }
  if (!bIsProperty && eLogicType == XFA_LOGIC_Transparent) {
    parent = ResolveNodes_GetParent(pNode, XFA_LOGIC_Transparent);
    if (!parent) {
      return 0;
    }
  }
  uint32_t dwHashName = pNode->GetNameHash();
  if (bIsClassIndex) {
    dwHashName = pNode->GetClassHashCode();
  }
  CXFA_NodeArray siblings;
  int32_t iSize = NodeAcc_TraverseSiblings(parent, dwHashName, &siblings,
                                           eLogicType, bIsClassIndex);
  for (int32_t i = 0; i < iSize; ++i) {
    CXFA_Node* child = siblings[i];
    if (child == pNode) {
      return i;
    }
  }
  return 0;
}

void CXFA_NodeHelper::GetNameExpression(CXFA_Node* refNode,
                                        CFX_WideString& wsName,
                                        bool bIsAllPath,
                                        XFA_LOGIC_TYPE eLogicType) {
  wsName.clear();
  if (bIsAllPath) {
    GetNameExpression(refNode, wsName, false, eLogicType);
    CFX_WideString wsParent;
    CXFA_Node* parent =
        ResolveNodes_GetParent(refNode, XFA_LOGIC_NoTransparent);
    while (parent) {
      GetNameExpression(parent, wsParent, false, eLogicType);
      wsParent += L".";
      wsParent += wsName;
      wsName = wsParent;
      parent = ResolveNodes_GetParent(parent, XFA_LOGIC_NoTransparent);
    }
    return;
  }

  CFX_WideString ws;
  bool bIsProperty = NodeIsProperty(refNode);
  if (refNode->IsUnnamed() ||
      (bIsProperty && refNode->GetElementType() != XFA_Element::PageSet)) {
    ws = refNode->GetClassName();
    wsName.Format(L"#%s[%d]", ws.c_str(),
                  GetIndex(refNode, eLogicType, bIsProperty, true));
    return;
  }
  ws = refNode->GetCData(XFA_ATTRIBUTE_Name);
  ws.Replace(L".", L"\\.");
  wsName.Format(L"%s[%d]", ws.c_str(),
                GetIndex(refNode, eLogicType, bIsProperty, false));
}

bool CXFA_NodeHelper::NodeIsTransparent(CXFA_Node* refNode) {
  if (!refNode) {
    return false;
  }
  XFA_Element refNodeType = refNode->GetElementType();
  if ((refNode->IsUnnamed() && refNode->IsContainerNode()) ||
      refNodeType == XFA_Element::SubformSet ||
      refNodeType == XFA_Element::Area || refNodeType == XFA_Element::Proto) {
    return true;
  }
  return false;
}

bool CXFA_NodeHelper::CreateNode_ForCondition(CFX_WideString& wsCondition) {
  int32_t iLen = wsCondition.GetLength();
  CFX_WideString wsIndex(L"0");
  bool bAll = false;
  if (iLen == 0) {
    m_iCreateFlag = XFA_RESOLVENODE_RSTYPE_CreateNodeOne;
    return false;
  }
  if (wsCondition.GetAt(0) == '[') {
    int32_t i = 1;
    for (; i < iLen; ++i) {
      FX_WCHAR ch = wsCondition[i];
      if (ch == ' ') {
        continue;
      }
      if (ch == '+' || ch == '-') {
        break;
      } else if (ch == '*') {
        bAll = true;
        break;
      } else {
        break;
      }
    }
    if (bAll) {
      wsIndex = FX_WSTRC(L"1");
      m_iCreateFlag = XFA_RESOLVENODE_RSTYPE_CreateNodeAll;
    } else {
      m_iCreateFlag = XFA_RESOLVENODE_RSTYPE_CreateNodeOne;
      wsIndex = wsCondition.Mid(i, iLen - 1 - i);
    }
    int32_t iIndex = wsIndex.GetInteger();
    m_iCreateCount = iIndex;
    return true;
  }
  return false;
}

bool CXFA_NodeHelper::ResolveNodes_CreateNode(
    CFX_WideString wsName,
    CFX_WideString wsCondition,
    bool bLastNode,
    CXFA_ScriptContext* pScriptContext) {
  if (!m_pCreateParent) {
    return false;
  }
  bool bIsClassName = false;
  bool bResult = false;
  if (wsName.GetAt(0) == '!') {
    wsName = wsName.Right(wsName.GetLength() - 1);
    m_pCreateParent = ToNode(
        pScriptContext->GetDocument()->GetXFAObject(XFA_HASHCODE_Datasets));
  }
  if (wsName.GetAt(0) == '#') {
    bIsClassName = true;
    wsName = wsName.Right(wsName.GetLength() - 1);
  }
  if (m_iCreateCount == 0) {
    CreateNode_ForCondition(wsCondition);
  }
  if (bIsClassName) {
    XFA_Element eType = XFA_GetElementTypeForName(wsName.AsStringC());
    if (eType == XFA_Element::Unknown)
      return false;

    for (int32_t iIndex = 0; iIndex < m_iCreateCount; iIndex++) {
      CXFA_Node* pNewNode = m_pCreateParent->CreateSamePacketNode(eType);
      if (pNewNode) {
        m_pCreateParent->InsertChild(pNewNode);
        if (iIndex == m_iCreateCount - 1) {
          m_pCreateParent = pNewNode;
        }
        bResult = true;
      }
    }
  } else {
    XFA_Element eClassType = XFA_Element::DataGroup;
    if (bLastNode) {
      eClassType = m_eLastCreateType;
    }
    for (int32_t iIndex = 0; iIndex < m_iCreateCount; iIndex++) {
      CXFA_Node* pNewNode = m_pCreateParent->CreateSamePacketNode(eClassType);
      if (pNewNode) {
        pNewNode->SetAttribute(XFA_ATTRIBUTE_Name, wsName.AsStringC());
        pNewNode->CreateXMLMappingNode();
        m_pCreateParent->InsertChild(pNewNode);
        if (iIndex == m_iCreateCount - 1) {
          m_pCreateParent = pNewNode;
        }
        bResult = true;
      }
    }
  }
  if (!bResult) {
    m_pCreateParent = nullptr;
  }
  return bResult;
}

void CXFA_NodeHelper::SetCreateNodeType(CXFA_Node* refNode) {
  if (!refNode) {
    return;
  }
  if (refNode->GetElementType() == XFA_Element::Subform) {
    m_eLastCreateType = XFA_Element::DataGroup;
  } else if (refNode->GetElementType() == XFA_Element::Field) {
    m_eLastCreateType = XFA_FieldIsMultiListBox(refNode)
                            ? XFA_Element::DataGroup
                            : XFA_Element::DataValue;
  } else if (refNode->GetElementType() == XFA_Element::ExclGroup) {
    m_eLastCreateType = XFA_Element::DataValue;
  }
}

bool CXFA_NodeHelper::NodeIsProperty(CXFA_Node* refNode) {
  CXFA_Node* parent = ResolveNodes_GetParent(refNode, XFA_LOGIC_NoTransparent);
  return parent && refNode &&
         XFA_GetPropertyOfElement(parent->GetElementType(),
                                  refNode->GetElementType(),
                                  XFA_XDPPACKET_UNKNOWN);
}
