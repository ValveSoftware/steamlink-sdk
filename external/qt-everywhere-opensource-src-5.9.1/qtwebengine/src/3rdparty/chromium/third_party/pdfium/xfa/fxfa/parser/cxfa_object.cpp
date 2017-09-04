// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/xfa_object.h"

#include "core/fxcrt/fx_ext.h"
#include "fxjs/cfxjse_value.h"
#include "xfa/fxfa/app/xfa_ffnotify.h"
#include "xfa/fxfa/parser/cxfa_document.h"

CXFA_Object::CXFA_Object(CXFA_Document* pDocument,
                         XFA_ObjectType objectType,
                         XFA_Element elementType,
                         const CFX_WideStringC& elementName)
    : m_pDocument(pDocument),
      m_objectType(objectType),
      m_elementType(elementType),
      m_elementNameHash(FX_HashCode_GetW(elementName, false)),
      m_elementName(elementName) {}

CXFA_Object::~CXFA_Object() {}

CFX_WideStringC CXFA_Object::GetClassName() const {
  return m_elementName;
}

uint32_t CXFA_Object::GetClassHashCode() const {
  return m_elementNameHash;
}

XFA_Element CXFA_Object::GetElementType() const {
  return m_elementType;
}

void CXFA_Object::Script_ObjectClass_ClassName(CFXJSE_Value* pValue,
                                               bool bSetting,
                                               XFA_ATTRIBUTE eAttribute) {
  if (bSetting) {
    ThrowException(XFA_IDS_INVAlID_PROP_SET);
    return;
  }
  CFX_WideStringC className = GetClassName();
  pValue->SetString(
      FX_UTF8Encode(className.c_str(), className.GetLength()).AsStringC());
}

void CXFA_Object::ThrowException(int32_t iStringID, ...) {
  IXFA_AppProvider* pAppProvider = m_pDocument->GetNotify()->GetAppProvider();
  ASSERT(pAppProvider);
  CFX_WideString wsFormat;
  pAppProvider->LoadString(iStringID, wsFormat);
  CFX_WideString wsMessage;
  va_list arg_ptr;
  va_start(arg_ptr, iStringID);
  wsMessage.FormatV(wsFormat.c_str(), arg_ptr);
  va_end(arg_ptr);
  FXJSE_ThrowMessage(
      FX_UTF8Encode(wsMessage.c_str(), wsMessage.GetLength()).AsStringC());
}
