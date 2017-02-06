// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/xfa_utils.h"

#include "core/fxcrt/include/fx_ext.h"
#include "xfa/fde/xml/fde_xml_imp.h"
#include "xfa/fxfa/parser/xfa_doclayout.h"
#include "xfa/fxfa/parser/xfa_document.h"
#include "xfa/fxfa/parser/xfa_localemgr.h"
#include "xfa/fxfa/parser/xfa_localevalue.h"
#include "xfa/fxfa/parser/xfa_object.h"
#include "xfa/fxfa/parser/xfa_parser.h"
#include "xfa/fxfa/parser/xfa_script.h"

CXFA_Node* XFA_CreateUIChild(CXFA_Node* pNode, XFA_Element& eWidgetType) {
  XFA_Element eType = pNode->GetElementType();
  eWidgetType = eType;
  if (eType != XFA_Element::Field && eType != XFA_Element::Draw) {
    return nullptr;
  }
  eWidgetType = XFA_Element::Unknown;
  XFA_Element eUIType = XFA_Element::Unknown;
  CXFA_Value defValue(pNode->GetProperty(0, XFA_Element::Value, TRUE));
  XFA_Element eValueType = defValue.GetChildValueClassID();
  switch (eValueType) {
    case XFA_Element::Boolean:
      eUIType = XFA_Element::CheckButton;
      break;
    case XFA_Element::Integer:
    case XFA_Element::Decimal:
    case XFA_Element::Float:
      eUIType = XFA_Element::NumericEdit;
      break;
    case XFA_Element::ExData:
    case XFA_Element::Text:
      eUIType = XFA_Element::TextEdit;
      eWidgetType = XFA_Element::Text;
      break;
    case XFA_Element::Date:
    case XFA_Element::Time:
    case XFA_Element::DateTime:
      eUIType = XFA_Element::DateTimeEdit;
      break;
    case XFA_Element::Image:
      eUIType = XFA_Element::ImageEdit;
      eWidgetType = XFA_Element::Image;
      break;
    case XFA_Element::Arc:
    case XFA_Element::Line:
    case XFA_Element::Rectangle:
      eUIType = XFA_Element::DefaultUi;
      eWidgetType = eValueType;
      break;
    default:
      break;
  }
  CXFA_Node* pUIChild = nullptr;
  CXFA_Node* pUI = pNode->GetProperty(0, XFA_Element::Ui, TRUE);
  CXFA_Node* pChild = pUI->GetNodeItem(XFA_NODEITEM_FirstChild);
  for (; pChild; pChild = pChild->GetNodeItem(XFA_NODEITEM_NextSibling)) {
    XFA_Element eChildType = pChild->GetElementType();
    if (eChildType == XFA_Element::Extras ||
        eChildType == XFA_Element::Picture) {
      continue;
    }
    const XFA_PROPERTY* pProperty = XFA_GetPropertyOfElement(
        XFA_Element::Ui, eChildType, XFA_XDPPACKET_Form);
    if (pProperty && (pProperty->uFlags & XFA_PROPERTYFLAG_OneOf)) {
      pUIChild = pChild;
      break;
    }
  }
  if (eType == XFA_Element::Draw) {
    XFA_Element eDraw =
        pUIChild ? pUIChild->GetElementType() : XFA_Element::Unknown;
    switch (eDraw) {
      case XFA_Element::TextEdit:
        eWidgetType = XFA_Element::Text;
        break;
      case XFA_Element::ImageEdit:
        eWidgetType = XFA_Element::Image;
        break;
      default:
        eWidgetType = eWidgetType == XFA_Element::Unknown ? XFA_Element::Text
                                                          : eWidgetType;
        break;
    }
  } else {
    if (pUIChild && pUIChild->GetElementType() == XFA_Element::DefaultUi) {
      eWidgetType = XFA_Element::TextEdit;
    } else {
      eWidgetType =
          pUIChild ? pUIChild->GetElementType()
                   : (eUIType == XFA_Element::Unknown ? XFA_Element::TextEdit
                                                      : eUIType);
    }
  }
  if (!pUIChild) {
    if (eUIType == XFA_Element::Unknown) {
      eUIType = XFA_Element::TextEdit;
      defValue.GetNode()->GetProperty(0, XFA_Element::Text, TRUE);
    }
    pUIChild = pUI->GetProperty(0, eUIType, TRUE);
  } else if (eUIType == XFA_Element::Unknown) {
    switch (pUIChild->GetElementType()) {
      case XFA_Element::CheckButton: {
        eValueType = XFA_Element::Text;
        if (CXFA_Node* pItems = pNode->GetChild(0, XFA_Element::Items)) {
          if (CXFA_Node* pItem = pItems->GetChild(0, XFA_Element::Unknown)) {
            eValueType = pItem->GetElementType();
          }
        }
      } break;
      case XFA_Element::DateTimeEdit:
        eValueType = XFA_Element::DateTime;
        break;
      case XFA_Element::ImageEdit:
        eValueType = XFA_Element::Image;
        break;
      case XFA_Element::NumericEdit:
        eValueType = XFA_Element::Float;
        break;
      case XFA_Element::ChoiceList: {
        eValueType = (pUIChild->GetEnum(XFA_ATTRIBUTE_Open) ==
                      XFA_ATTRIBUTEENUM_MultiSelect)
                         ? XFA_Element::ExData
                         : XFA_Element::Text;
      } break;
      case XFA_Element::Barcode:
      case XFA_Element::Button:
      case XFA_Element::PasswordEdit:
      case XFA_Element::Signature:
      case XFA_Element::TextEdit:
      default:
        eValueType = XFA_Element::Text;
        break;
    }
    defValue.GetNode()->GetProperty(0, eValueType, TRUE);
  }
  return pUIChild;
}
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
FX_BOOL XFA_FieldIsMultiListBox(CXFA_Node* pFieldNode) {
  FX_BOOL bRet = FALSE;
  if (!pFieldNode) {
    return bRet;
  }
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
FX_BOOL XFA_IsLayoutElement(XFA_Element eElement, FX_BOOL bLayoutContainer) {
  switch (eElement) {
    case XFA_Element::Draw:
    case XFA_Element::Field:
    case XFA_Element::InstanceManager:
      return !bLayoutContainer;
    case XFA_Element::Area:
    case XFA_Element::Subform:
    case XFA_Element::ExclGroup:
    case XFA_Element::SubformSet:
    case XFA_Element::PageArea:
    case XFA_Element::Form:
      return TRUE;
    default:
      return FALSE;
  }
}

static const FX_DOUBLE fraction_scales[] = {0.1,
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
FX_DOUBLE XFA_WideStringToDouble(const CFX_WideString& wsStringVal) {
  CFX_WideString wsValue = wsStringVal;
  wsValue.TrimLeft();
  wsValue.TrimRight();
  int64_t nIntegral = 0;
  uint32_t dwFractional = 0;
  int32_t nExponent = 0;
  int32_t cc = 0;
  FX_BOOL bNegative = FALSE, bExpSign = FALSE;
  const FX_WCHAR* str = wsValue.c_str();
  int32_t len = wsValue.GetLength();
  if (str[0] == '+') {
    cc++;
  } else if (str[0] == '-') {
    bNegative = TRUE;
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
        bExpSign = TRUE;
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

FX_DOUBLE XFA_ByteStringToDouble(const CFX_ByteStringC& szStringVal) {
  CFX_WideString wsValue = CFX_WideString::FromUTF8(szStringVal);
  return XFA_WideStringToDouble(wsValue);
}

int32_t XFA_MapRotation(int32_t nRotation) {
  nRotation = nRotation % 360;
  nRotation = nRotation < 0 ? nRotation + 360 : nRotation;
  return nRotation;
}
