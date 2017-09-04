// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cscript_signaturepseudomodel.h"

#include "fxjs/cfxjse_arguments.h"
#include "xfa/fxfa/app/xfa_ffnotify.h"
#include "xfa/fxfa/parser/cxfa_document.h"
#include "xfa/fxfa/parser/cxfa_scriptcontext.h"
#include "xfa/fxfa/parser/xfa_localemgr.h"
#include "xfa/fxfa/parser/xfa_object.h"
#include "xfa/fxfa/parser/xfa_utils.h"

CScript_SignaturePseudoModel::CScript_SignaturePseudoModel(
    CXFA_Document* pDocument)
    : CXFA_Object(pDocument,
                  XFA_ObjectType::Object,
                  XFA_Element::SignaturePseudoModel,
                  CFX_WideStringC(L"signaturePseudoModel")) {}

CScript_SignaturePseudoModel::~CScript_SignaturePseudoModel() {}

void CScript_SignaturePseudoModel::Verify(CFXJSE_Arguments* pArguments) {
  int32_t iLength = pArguments->GetLength();
  if (iLength < 1 || iLength > 4) {
    ThrowException(XFA_IDS_INCORRECT_NUMBER_OF_METHOD, L"verify");
    return;
  }

  CFXJSE_Value* pValue = pArguments->GetReturnValue();
  if (pValue)
    pValue->SetInteger(0);
}

void CScript_SignaturePseudoModel::Sign(CFXJSE_Arguments* pArguments) {
  int32_t iLength = pArguments->GetLength();
  if (iLength < 3 || iLength > 7) {
    ThrowException(XFA_IDS_INCORRECT_NUMBER_OF_METHOD, L"sign");
    return;
  }

  CFXJSE_Value* pValue = pArguments->GetReturnValue();
  if (pValue)
    pValue->SetBoolean(false);
}

void CScript_SignaturePseudoModel::Enumerate(CFXJSE_Arguments* pArguments) {
  if (pArguments->GetLength() != 0) {
    ThrowException(XFA_IDS_INCORRECT_NUMBER_OF_METHOD, L"enumerate");
    return;
  }
  return;
}

void CScript_SignaturePseudoModel::Clear(CFXJSE_Arguments* pArguments) {
  int32_t iLength = pArguments->GetLength();
  if (iLength < 1 || iLength > 2) {
    ThrowException(XFA_IDS_INCORRECT_NUMBER_OF_METHOD, L"clear");
    return;
  }

  CFXJSE_Value* pValue = pArguments->GetReturnValue();
  if (pValue)
    pValue->SetBoolean(false);
}
