// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_XFA_UTILS_H_
#define XFA_FXFA_PARSER_XFA_UTILS_H_

#include "xfa/fde/xml/fde_xml.h"
#include "xfa/fgas/crt/fgas_stream.h"
#include "xfa/fgas/crt/fgas_utils.h"
#include "xfa/fxfa/fxfa_basic.h"

class CFDE_XMLElement;
class CFDE_XMLNode;
class CXFA_LocaleValue;
class CXFA_Node;
class CXFA_WidgetData;

bool XFA_FDEExtension_ResolveNamespaceQualifier(
    CFDE_XMLElement* pNode,
    const CFX_WideStringC& wsQualifier,
    CFX_WideString& wsNamespaceURI);

template <class NodeType, class TraverseStrategy>
class CXFA_NodeIteratorTemplate {
 public:
  CXFA_NodeIteratorTemplate(NodeType* pRootNode = nullptr)
      : m_pRoot(pRootNode), m_NodeStack(100) {
    if (pRootNode) {
      m_NodeStack.Push(pRootNode);
    }
  }
  bool Init(NodeType* pRootNode) {
    if (!pRootNode) {
      return false;
    }
    m_pRoot = pRootNode;
    m_NodeStack.RemoveAll(false);
    m_NodeStack.Push(pRootNode);
    return true;
  }
  void Clear() { m_NodeStack.RemoveAll(false); }
  void Reset() {
    Clear();
    if (m_pRoot) {
      m_NodeStack.Push(m_pRoot);
    }
  }
  bool SetCurrent(NodeType* pCurNode) {
    m_NodeStack.RemoveAll(false);
    if (pCurNode) {
      CFX_StackTemplate<NodeType*> revStack(100);
      NodeType* pNode;
      for (pNode = pCurNode; pNode && pNode != m_pRoot;
           pNode = TraverseStrategy::GetParent(pNode)) {
        revStack.Push(pNode);
      }
      if (!pNode) {
        return false;
      }
      revStack.Push(m_pRoot);
      while (revStack.GetSize()) {
        m_NodeStack.Push(*revStack.GetTopElement());
        revStack.Pop();
      }
    }
    return true;
  }
  NodeType* GetCurrent() const {
    return m_NodeStack.GetSize() ? *m_NodeStack.GetTopElement() : nullptr;
  }
  NodeType* GetRoot() const { return m_pRoot; }
  NodeType* MoveToPrev() {
    int32_t nStackLength = m_NodeStack.GetSize();
    if (nStackLength == 1) {
      return nullptr;
    } else if (nStackLength > 1) {
      NodeType* pCurItem = *m_NodeStack.GetTopElement();
      m_NodeStack.Pop();
      NodeType* pParentItem = *m_NodeStack.GetTopElement();
      NodeType* pParentFirstChildItem =
          TraverseStrategy::GetFirstChild(pParentItem);
      if (pCurItem == pParentFirstChildItem) {
        return pParentItem;
      }
      NodeType *pPrevItem = pParentFirstChildItem, *pPrevItemNext = nullptr;
      for (; pPrevItem; pPrevItem = pPrevItemNext) {
        pPrevItemNext = TraverseStrategy::GetNextSibling(pPrevItem);
        if (!pPrevItemNext || pPrevItemNext == pCurItem) {
          break;
        }
      }
      m_NodeStack.Push(pPrevItem);
    } else {
      m_NodeStack.RemoveAll(false);
      if (m_pRoot) {
        m_NodeStack.Push(m_pRoot);
      }
    }
    if (m_NodeStack.GetSize() > 0) {
      NodeType* pChildItem = *m_NodeStack.GetTopElement();
      while ((pChildItem = TraverseStrategy::GetFirstChild(pChildItem)) !=
             nullptr) {
        while (NodeType* pNextItem =
                   TraverseStrategy::GetNextSibling(pChildItem)) {
          pChildItem = pNextItem;
        }
        m_NodeStack.Push(pChildItem);
      }
      return *m_NodeStack.GetTopElement();
    }
    return nullptr;
  }
  NodeType* MoveToNext() {
    NodeType** ppNode = nullptr;
    NodeType* pCurrent = GetCurrent();
    while (m_NodeStack.GetSize() > 0) {
      while ((ppNode = m_NodeStack.GetTopElement()) != nullptr) {
        if (pCurrent != *ppNode) {
          return *ppNode;
        }
        NodeType* pChild = TraverseStrategy::GetFirstChild(*ppNode);
        if (!pChild) {
          break;
        }
        m_NodeStack.Push(pChild);
      }
      while ((ppNode = m_NodeStack.GetTopElement()) != nullptr) {
        NodeType* pNext = TraverseStrategy::GetNextSibling(*ppNode);
        m_NodeStack.Pop();
        if (m_NodeStack.GetSize() == 0) {
          break;
        }
        if (pNext) {
          m_NodeStack.Push(pNext);
          break;
        }
      }
    }
    return nullptr;
  }
  NodeType* SkipChildrenAndMoveToNext() {
    NodeType** ppNode = nullptr;
    while ((ppNode = m_NodeStack.GetTopElement()) != nullptr) {
      NodeType* pNext = TraverseStrategy::GetNextSibling(*ppNode);
      m_NodeStack.Pop();
      if (m_NodeStack.GetSize() == 0) {
        break;
      }
      if (pNext) {
        m_NodeStack.Push(pNext);
        break;
      }
    }
    return GetCurrent();
  }

 protected:
  NodeType* m_pRoot;
  CFX_StackTemplate<NodeType*> m_NodeStack;
};

CXFA_LocaleValue XFA_GetLocaleValue(CXFA_WidgetData* pWidgetData);
FX_DOUBLE XFA_ByteStringToDouble(const CFX_ByteStringC& szStringVal);
int32_t XFA_MapRotation(int32_t nRotation);

bool XFA_RecognizeRichText(CFDE_XMLElement* pRichTextXMLNode);
void XFA_GetPlainTextFromRichText(CFDE_XMLNode* pXMLNode,
                                  CFX_WideString& wsPlainText);
bool XFA_FieldIsMultiListBox(CXFA_Node* pFieldNode);

void XFA_DataExporter_DealWithDataGroupNode(CXFA_Node* pDataNode);
void XFA_DataExporter_RegenerateFormFile(CXFA_Node* pNode,
                                         IFX_Stream* pStream,
                                         const FX_CHAR* pChecksum = nullptr,
                                         bool bSaveXML = false);

const XFA_NOTSUREATTRIBUTE* XFA_GetNotsureAttribute(
    XFA_Element eElement,
    XFA_ATTRIBUTE eAttribute,
    XFA_ATTRIBUTETYPE eType = XFA_ATTRIBUTETYPE_NOTSURE);

const XFA_SCRIPTATTRIBUTEINFO* XFA_GetScriptAttributeByName(
    XFA_Element eElement,
    const CFX_WideStringC& wsAttributeName);

const XFA_PROPERTY* XFA_GetPropertyOfElement(XFA_Element eElement,
                                             XFA_Element eProperty,
                                             uint32_t dwPacket);
const XFA_PROPERTY* XFA_GetElementProperties(XFA_Element eElement,
                                             int32_t& iCount);
const uint8_t* XFA_GetElementAttributes(XFA_Element eElement, int32_t& iCount);
const XFA_ELEMENTINFO* XFA_GetElementByID(XFA_Element eName);
XFA_Element XFA_GetElementTypeForName(const CFX_WideStringC& wsName);
CXFA_Measurement XFA_GetAttributeDefaultValue_Measure(XFA_Element eElement,
                                                      XFA_ATTRIBUTE eAttribute,
                                                      uint32_t dwPacket);
bool XFA_GetAttributeDefaultValue(void*& pValue,
                                  XFA_Element eElement,
                                  XFA_ATTRIBUTE eAttribute,
                                  XFA_ATTRIBUTETYPE eType,
                                  uint32_t dwPacket);
const XFA_ATTRIBUTEINFO* XFA_GetAttributeByName(const CFX_WideStringC& wsName);
const XFA_ATTRIBUTEINFO* XFA_GetAttributeByID(XFA_ATTRIBUTE eName);
const XFA_ATTRIBUTEENUMINFO* XFA_GetAttributeEnumByName(
    const CFX_WideStringC& wsName);
const XFA_PACKETINFO* XFA_GetPacketByIndex(XFA_PACKET ePacket);
const XFA_PACKETINFO* XFA_GetPacketByID(uint32_t dwPacket);

#endif  // XFA_FXFA_PARSER_XFA_UTILS_H_
