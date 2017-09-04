// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/xfa_ffdochandler.h"

#include "xfa/fxfa/parser/cxfa_scriptcontext.h"
#include "xfa/fxfa/parser/xfa_resolvenode_rs.h"
#include "xfa/fxfa/xfa_checksum.h"
#include "xfa/fxfa/xfa_ffdoc.h"

CXFA_FFDocHandler::CXFA_FFDocHandler() {}

CXFA_FFDocHandler::~CXFA_FFDocHandler() {}

CFXJSE_Value* CXFA_FFDocHandler::GetXFAScriptObject(CXFA_FFDoc* hDoc) {
  CXFA_Document* pXFADoc = hDoc->GetXFADoc();
  if (!pXFADoc)
    return nullptr;

  CXFA_ScriptContext* pScriptContext = pXFADoc->GetScriptContext();
  if (!pScriptContext)
    return nullptr;
  return pScriptContext->GetJSValueFromMap(pXFADoc->GetRoot());
}

XFA_ATTRIBUTEENUM CXFA_FFDocHandler::GetRestoreState(CXFA_FFDoc* hDoc) {
  CXFA_Document* pXFADoc = hDoc->GetXFADoc();
  if (!pXFADoc)
    return XFA_ATTRIBUTEENUM_Unknown;

  CXFA_Node* pForm = ToNode(pXFADoc->GetXFAObject(XFA_HASHCODE_Form));
  if (!pForm)
    return XFA_ATTRIBUTEENUM_Unknown;

  CXFA_Node* pSubForm = pForm->GetFirstChildByClass(XFA_Element::Subform);
  if (!pSubForm)
    return XFA_ATTRIBUTEENUM_Unknown;
  return pSubForm->GetEnum(XFA_ATTRIBUTE_RestoreState);
}

bool CXFA_FFDocHandler::RunDocScript(CXFA_FFDoc* hDoc,
                                     XFA_SCRIPTTYPE eScriptType,
                                     const CFX_WideStringC& wsScript,
                                     CFXJSE_Value* pRetValue,
                                     CFXJSE_Value* pThisValue) {
  CXFA_Document* pXFADoc = hDoc->GetXFADoc();
  if (!pXFADoc)
    return false;

  CXFA_ScriptContext* pScriptContext = pXFADoc->GetScriptContext();
  if (!pScriptContext)
    return false;

  return pScriptContext->RunScript(
      (XFA_SCRIPTLANGTYPE)eScriptType, wsScript, pRetValue,
      pThisValue ? CXFA_ScriptContext::ToObject(pThisValue, nullptr) : nullptr);
}
