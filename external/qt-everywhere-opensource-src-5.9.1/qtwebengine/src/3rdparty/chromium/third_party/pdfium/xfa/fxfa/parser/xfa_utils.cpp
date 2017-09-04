// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/xfa_utils.h"

#include "core/fxcrt/fx_ext.h"
#include "xfa/fde/xml/fde_xml_imp.h"
#include "xfa/fxfa/parser/cxfa_document.h"
#include "xfa/fxfa/parser/cxfa_measurement.h"
#include "xfa/fxfa/parser/xfa_basic_data.h"
#include "xfa/fxfa/parser/xfa_localemgr.h"
#include "xfa/fxfa/parser/xfa_localevalue.h"
#include "xfa/fxfa/parser/xfa_object.h"

namespace {

const FX_DOUBLE fraction_scales[] = {0.1,
                                     0.01,
                                     0.001,
                                     0.0001,
                                     0.00001,
                                     0.000001,
                                     0.0000001,
                                     0.00000001,
                                     0.000000001,
                                     0.0000000001,
                                     0.00000000001,
                                     0.000000000001,
                                     0.0000000000001,
                                     0.00000000000001,
                                     0.000000000000001,
                                     0.0000000000000001};

FX_DOUBLE WideStringToDouble(const CFX_WideString& wsStringVal) {
  CFX_WideString wsValue = wsStringVal;
  wsValue.TrimLeft();
  wsValue.TrimRight();
  int64_t nIntegral = 0;
  uint32_t dwFractional = 0;
  int32_t nExponent = 0;
  int32_t cc = 0;
  bool bNegative = false;
  bool bExpSign = false;
  const FX_WCHAR* str = wsValue.c_str();
  int32_t len = wsValue.GetLength();
  if (str[0] == '+') {
    cc++;
  } else if (str[0] == '-') {
    bNegative = true;
    cc++;
  }
  int32_t nIntegralLen = 0;
  while (cc < len) {
    if (str[cc] == '.' || str[cc] == 'E' || str[cc] == 'e' ||
        nIntegralLen > 17) {
      break;
    }
    if (!FXSYS_isDecimalDigit(str[cc])) {
      return 0;
    }
    nIntegral = nIntegral * 10 + str[cc] - '0';
    cc++;
    nIntegralLen++;
  }
  nIntegral = bNegative ? -nIntegral : nIntegral;
  int32_t scale = 0;
  FX_DOUBLE fraction = 0.0;
  if (cc < len && str[cc] == '.') {
    cc++;
    while (cc < len) {
      fraction += fraction_scales[scale] * (str[cc] - '0');
      scale++;
      cc++;
      if (cc == len) {
        break;
      }
      if (scale == sizeof(fraction_scales) / sizeof(FX_DOUBLE) ||
          str[cc] == 'E' || str[cc] == 'e') {
        break;
      }
      if (!FXSYS_isDecimalDigit(str[cc])) {
        return 0;
      }
    }
    dwFractional = (uint32_t)(fraction * 4294967296.0);
  }
  if (cc < len && (str[cc] == 'E' || str[cc] == 'e')) {
    cc++;
    if (cc < len) {
      if (str[cc] == '+') {
        cc++;
      } else if (str[cc] == '-') {
        bExpSign = true;
        cc++;
      }
    }
    while (cc < len) {
      if (str[cc] == '.' || !FXSYS_isDecimalDigit(str[cc])) {
        return 0;
      }
      nExponent = nExponent * 10 + str[cc] - '0';
      cc++;
    }
    nExponent = bExpSign ? -nExponent : nExponent;
  }
  FX_DOUBLE dValue = (dwFractional / 4294967296.0);
  dValue = nIntegral + (nIntegral >= 0 ? dValue : -dValue);
  if (nExponent != 0) {
    dValue *= FXSYS_pow(10, (FX_FLOAT)nExponent);
  }
  return dValue;
}

}  // namespace

CXFA_LocaleValue XFA_GetLocaleValue(CXFA_WidgetData* pWidgetData) {
  CXFA_Node* pNodeValue =
      pWidgetData->GetNode()->GetChild(0, XFA_Element::Value);
  if (!pNodeValue) {
    return CXFA_LocaleValue();
  }
  CXFA_Node* pValueChild = pNodeValue->GetNodeItem(XFA_NODEITEM_FirstChild);
  if (!pValueChild) {
    return CXFA_LocaleValue();
  }
  int32_t iVTType = XFA_VT_NULL;
  switch (pValueChild->GetElementType()) {
    case XFA_Element::Decimal:
      iVTType = XFA_VT_DECIMAL;
      break;
    case XFA_Element::Float:
      iVTType = XFA_VT_FLOAT;
      break;
    case XFA_Element::Date:
      iVTType = XFA_VT_DATE;
      break;
    case XFA_Element::Time:
      iVTType = XFA_VT_TIME;
      break;
    case XFA_Element::DateTime:
      iVTType = XFA_VT_DATETIME;
      break;
    case XFA_Element::Boolean:
      iVTType = XFA_VT_BOOLEAN;
      break;
    case XFA_Element::Integer:
      iVTType = XFA_VT_INTEGER;
      break;
    case XFA_Element::Text:
      iVTType = XFA_VT_TEXT;
      break;
    default:
      iVTType = XFA_VT_NULL;
      break;
  }
  return CXFA_LocaleValue(iVTType, pWidgetData->GetRawValue(),
                          pWidgetData->GetNode()->GetDocument()->GetLocalMgr());
}
void XFA_GetPlainTextFromRichText(CFDE_XMLNode* pXMLNode,
                                  CFX_WideString& wsPlainText) {
  if (!pXMLNode) {
    return;
  }
  switch (pXMLNode->GetType()) {
    case FDE_XMLNODE_Element: {
      CFDE_XMLElement* pXMLElement = static_cast<CFDE_XMLElement*>(pXMLNode);
      CFX_WideString wsTag;
      pXMLElement->GetLocalTagName(wsTag);
      uint32_t uTag = FX_HashCode_GetW(wsTag.AsStringC(), true);
      if (uTag == 0x0001f714) {
        wsPlainText += L"\n";
      } else if (uTag == 0x00000070) {
        if (!wsPlainText.IsEmpty()) {
          wsPlainText += L"\n";
        }
      } else if (uTag == 0xa48ac63) {
        if (!wsPlainText.IsEmpty() &&
            wsPlainText[wsPlainText.GetLength() - 1] != '\n') {
          wsPlainText += L"\n";
        }
      }
    } break;
    case FDE_XMLNODE_Text: {
      CFX_WideString wsContent;
      static_cast<CFDE_XMLText*>(pXMLNode)->GetText(wsContent);
      wsPlainText += wsContent;
    } break;
    case FDE_XMLNODE_CharData: {
      CFX_WideString wsCharData;
      static_cast<CFDE_XMLCharData*>(pXMLNode)->GetCharData(wsCharData);
      wsPlainText += wsCharData;
    } break;
    default:
      break;
  }
  for (CFDE_XMLNode* pChildXML =
           pXMLNode->GetNodeItem(CFDE_XMLNode::FirstChild);
       pChildXML;
       pChildXML = pChildXML->GetNodeItem(CFDE_XMLNode::NextSibling)) {
    XFA_GetPlainTextFromRichText(pChildXML, wsPlainText);
  }
}

bool XFA_FieldIsMultiListBox(CXFA_Node* pFieldNode) {
  bool bRet = false;
  if (!pFieldNode)
    return bRet;

  CXFA_Node* pUIChild = pFieldNode->GetChild(0, XFA_Element::Ui);
  if (pUIChild) {
    CXFA_Node* pFirstChild = pUIChild->GetNodeItem(XFA_NODEITEM_FirstChild);
    if (pFirstChild &&
        pFirstChild->GetElementType() == XFA_Element::ChoiceList) {
      bRet = pFirstChild->GetEnum(XFA_ATTRIBUTE_Open) ==
             XFA_ATTRIBUTEENUM_MultiSelect;
    }
  }
  return bRet;
}

FX_DOUBLE XFA_ByteStringToDouble(const CFX_ByteStringC& szStringVal) {
  CFX_WideString wsValue = CFX_WideString::FromUTF8(szStringVal);
  return WideStringToDouble(wsValue);
}

int32_t XFA_MapRotation(int32_t nRotation) {
  nRotation = nRotation % 360;
  nRotation = nRotation < 0 ? nRotation + 360 : nRotation;
  return nRotation;
}

const XFA_SCRIPTATTRIBUTEINFO* XFA_GetScriptAttributeByName(
    XFA_Element eElement,
    const CFX_WideStringC& wsAttributeName) {
  if (wsAttributeName.IsEmpty())
    return nullptr;

  int32_t iElementIndex = static_cast<int32_t>(eElement);
  while (iElementIndex != -1) {
    const XFA_SCRIPTHIERARCHY* scriptIndex = g_XFAScriptIndex + iElementIndex;
    int32_t icount = scriptIndex->wAttributeCount;
    if (icount == 0) {
      iElementIndex = scriptIndex->wParentIndex;
      continue;
    }
    uint32_t uHash = FX_HashCode_GetW(wsAttributeName, false);
    int32_t iStart = scriptIndex->wAttributeStart, iEnd = iStart + icount - 1;
    do {
      int32_t iMid = (iStart + iEnd) / 2;
      const XFA_SCRIPTATTRIBUTEINFO* pInfo = g_SomAttributeData + iMid;
      if (uHash == pInfo->uHash)
        return pInfo;
      if (uHash < pInfo->uHash)
        iEnd = iMid - 1;
      else
        iStart = iMid + 1;
    } while (iStart <= iEnd);
    iElementIndex = scriptIndex->wParentIndex;
  }
  return nullptr;
}

const XFA_NOTSUREATTRIBUTE* XFA_GetNotsureAttribute(XFA_Element eElement,
                                                    XFA_ATTRIBUTE eAttribute,
                                                    XFA_ATTRIBUTETYPE eType) {
  int32_t iStart = 0, iEnd = g_iXFANotsureCount - 1;
  do {
    int32_t iMid = (iStart + iEnd) / 2;
    const XFA_NOTSUREATTRIBUTE* pAttr = g_XFANotsureAttributes + iMid;
    if (eElement == pAttr->eElement) {
      if (pAttr->eAttribute == eAttribute) {
        if (eType == XFA_ATTRIBUTETYPE_NOTSURE || eType == pAttr->eType)
          return pAttr;
        return nullptr;
      }
      int32_t iBefore = iMid - 1;
      if (iBefore >= 0) {
        pAttr = g_XFANotsureAttributes + iBefore;
        while (eElement == pAttr->eElement) {
          if (pAttr->eAttribute == eAttribute) {
            if (eType == XFA_ATTRIBUTETYPE_NOTSURE || eType == pAttr->eType)
              return pAttr;
            return nullptr;
          }
          iBefore--;
          if (iBefore < 0)
            break;

          pAttr = g_XFANotsureAttributes + iBefore;
        }
      }

      int32_t iAfter = iMid + 1;
      if (iAfter <= g_iXFANotsureCount - 1) {
        pAttr = g_XFANotsureAttributes + iAfter;
        while (eElement == pAttr->eElement) {
          if (pAttr->eAttribute == eAttribute) {
            if (eType == XFA_ATTRIBUTETYPE_NOTSURE || eType == pAttr->eType)
              return pAttr;
            return nullptr;
          }
          iAfter++;
          if (iAfter > g_iXFANotsureCount - 1)
            break;

          pAttr = g_XFANotsureAttributes + iAfter;
        }
      }
      return nullptr;
    }

    if (eElement < pAttr->eElement)
      iEnd = iMid - 1;
    else
      iStart = iMid + 1;
  } while (iStart <= iEnd);
  return nullptr;
}

const XFA_PROPERTY* XFA_GetPropertyOfElement(XFA_Element eElement,
                                             XFA_Element eProperty,
                                             uint32_t dwPacket) {
  int32_t iCount = 0;
  const XFA_PROPERTY* pProperties = XFA_GetElementProperties(eElement, iCount);
  if (!pProperties || iCount < 1)
    return nullptr;

  auto it = std::find_if(pProperties, pProperties + iCount,
                         [eProperty](const XFA_PROPERTY& prop) {
                           return prop.eName == eProperty;
                         });
  if (it == pProperties + iCount)
    return nullptr;

  const XFA_ELEMENTINFO* pInfo = XFA_GetElementByID(eProperty);
  ASSERT(pInfo);
  if (dwPacket != XFA_XDPPACKET_UNKNOWN && !(dwPacket & pInfo->dwPackets))
    return nullptr;
  return it;
}

const XFA_PROPERTY* XFA_GetElementProperties(XFA_Element eElement,
                                             int32_t& iCount) {
  if (eElement == XFA_Element::Unknown)
    return nullptr;

  const XFA_ELEMENTHIERARCHY* pElement =
      g_XFAElementPropertyIndex + static_cast<int32_t>(eElement);
  iCount = pElement->wCount;
  return g_XFAElementPropertyData + pElement->wStart;
}

const uint8_t* XFA_GetElementAttributes(XFA_Element eElement, int32_t& iCount) {
  if (eElement == XFA_Element::Unknown)
    return nullptr;

  const XFA_ELEMENTHIERARCHY* pElement =
      g_XFAElementAttributeIndex + static_cast<int32_t>(eElement);
  iCount = pElement->wCount;
  return g_XFAElementAttributeData + pElement->wStart;
}

const XFA_ELEMENTINFO* XFA_GetElementByID(XFA_Element eName) {
  return eName != XFA_Element::Unknown
             ? g_XFAElementData + static_cast<int32_t>(eName)
             : nullptr;
}

XFA_Element XFA_GetElementTypeForName(const CFX_WideStringC& wsName) {
  if (wsName.IsEmpty())
    return XFA_Element::Unknown;

  uint32_t uHash = FX_HashCode_GetW(wsName, false);
  const XFA_ELEMENTINFO* pEnd = g_XFAElementData + g_iXFAElementCount;
  auto pInfo = std::lower_bound(g_XFAElementData, pEnd, uHash,
                                [](const XFA_ELEMENTINFO& info, uint32_t hash) {
                                  return info.uHash < hash;
                                });
  if (pInfo < pEnd && pInfo->uHash == uHash)
    return pInfo->eName;
  return XFA_Element::Unknown;
}

CXFA_Measurement XFA_GetAttributeDefaultValue_Measure(XFA_Element eElement,
                                                      XFA_ATTRIBUTE eAttribute,
                                                      uint32_t dwPacket) {
  void* pValue;
  if (XFA_GetAttributeDefaultValue(pValue, eElement, eAttribute,
                                   XFA_ATTRIBUTETYPE_Measure, dwPacket)) {
    return *(CXFA_Measurement*)pValue;
  }
  return CXFA_Measurement();
}

bool XFA_GetAttributeDefaultValue(void*& pValue,
                                  XFA_Element eElement,
                                  XFA_ATTRIBUTE eAttribute,
                                  XFA_ATTRIBUTETYPE eType,
                                  uint32_t dwPacket) {
  const XFA_ATTRIBUTEINFO* pInfo = XFA_GetAttributeByID(eAttribute);
  if (!pInfo)
    return false;
  if (dwPacket && (dwPacket & pInfo->dwPackets) == 0)
    return false;
  if (pInfo->eType == eType) {
    pValue = pInfo->pDefValue;
    return true;
  }
  if (pInfo->eType == XFA_ATTRIBUTETYPE_NOTSURE) {
    const XFA_NOTSUREATTRIBUTE* pAttr =
        XFA_GetNotsureAttribute(eElement, eAttribute, eType);
    if (pAttr) {
      pValue = pAttr->pValue;
      return true;
    }
  }
  return false;
}

const XFA_ATTRIBUTEINFO* XFA_GetAttributeByName(const CFX_WideStringC& wsName) {
  if (wsName.IsEmpty())
    return nullptr;

  uint32_t uHash = FX_HashCode_GetW(wsName, false);
  int32_t iStart = 0;
  int32_t iEnd = g_iXFAAttributeCount - 1;
  do {
    int32_t iMid = (iStart + iEnd) / 2;
    const XFA_ATTRIBUTEINFO* pInfo = g_XFAAttributeData + iMid;
    if (uHash == pInfo->uHash)
      return pInfo;
    if (uHash < pInfo->uHash)
      iEnd = iMid - 1;
    else
      iStart = iMid + 1;
  } while (iStart <= iEnd);
  return nullptr;
}

const XFA_ATTRIBUTEINFO* XFA_GetAttributeByID(XFA_ATTRIBUTE eName) {
  return (eName < g_iXFAAttributeCount) ? (g_XFAAttributeData + eName)
                                        : nullptr;
}

const XFA_ATTRIBUTEENUMINFO* XFA_GetAttributeEnumByName(
    const CFX_WideStringC& wsName) {
  if (wsName.IsEmpty())
    return nullptr;

  uint32_t uHash = FX_HashCode_GetW(wsName, false);
  int32_t iStart = 0;
  int32_t iEnd = g_iXFAEnumCount - 1;
  do {
    int32_t iMid = (iStart + iEnd) / 2;
    const XFA_ATTRIBUTEENUMINFO* pInfo = g_XFAEnumData + iMid;
    if (uHash == pInfo->uHash)
      return pInfo;
    if (uHash < pInfo->uHash)
      iEnd = iMid - 1;
    else
      iStart = iMid + 1;
  } while (iStart <= iEnd);
  return nullptr;
}

const XFA_PACKETINFO* XFA_GetPacketByIndex(XFA_PACKET ePacket) {
  return g_XFAPacketData + ePacket;
}

const XFA_PACKETINFO* XFA_GetPacketByID(uint32_t dwPacket) {
  int32_t iStart = 0, iEnd = g_iXFAPacketCount - 1;
  do {
    int32_t iMid = (iStart + iEnd) / 2;
    uint32_t dwFind = (g_XFAPacketData + iMid)->eName;
    if (dwPacket == dwFind)
      return g_XFAPacketData + iMid;
    if (dwPacket < dwFind)
      iEnd = iMid - 1;
    else
      iStart = iMid + 1;
  } while (iStart <= iEnd);
  return nullptr;
}
