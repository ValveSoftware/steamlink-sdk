// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/xfa_script_eventpseudomodel.h"

#include "fxjse/include/cfxjse_arguments.h"
#include "xfa/fxfa/app/xfa_ffnotify.h"
#include "xfa/fxfa/include/cxfa_eventparam.h"
#include "xfa/fxfa/include/xfa_ffwidgethandler.h"
#include "xfa/fxfa/parser/xfa_doclayout.h"
#include "xfa/fxfa/parser/xfa_document.h"
#include "xfa/fxfa/parser/xfa_localemgr.h"
#include "xfa/fxfa/parser/xfa_object.h"
#include "xfa/fxfa/parser/xfa_parser.h"
#include "xfa/fxfa/parser/xfa_parser_imp.h"
#include "xfa/fxfa/parser/xfa_script.h"
#include "xfa/fxfa/parser/xfa_script_imp.h"
#include "xfa/fxfa/parser/xfa_utils.h"

CScript_EventPseudoModel::CScript_EventPseudoModel(CXFA_Document* pDocument)
    : CXFA_Object(pDocument,
                  XFA_ObjectType::Object,
                  XFA_Element::EventPseudoModel) {}
CScript_EventPseudoModel::~CScript_EventPseudoModel() {}
void Script_EventPseudoModel_StringProperty(CFXJSE_Value* pValue,
                                            CFX_WideString& wsValue,
                                            FX_BOOL bSetting) {
  if (bSetting) {
    wsValue = pValue->ToWideString();
  } else {
    pValue->SetString(FX_UTF8Encode(wsValue).AsStringC());
  }
}
void Script_EventPseudoModel_InterProperty(CFXJSE_Value* pValue,
                                           int32_t& iValue,
                                           FX_BOOL bSetting) {
  if (bSetting) {
    iValue = pValue->ToInteger();
  } else {
    pValue->SetInteger(iValue);
  }
}
void Script_EventPseudoModel_BooleanProperty(CFXJSE_Value* pValue,
                                             FX_BOOL& bValue,
                                             FX_BOOL bSetting) {
  if (bSetting)
    bValue = pValue->ToBoolean();
  else
    pValue->SetBoolean(bValue);
}

void CScript_EventPseudoModel::Script_EventPseudoModel_Property(
    CFXJSE_Value* pValue,
    XFA_Event dwFlag,
    FX_BOOL bSetting) {
  CXFA_ScriptContext* pScriptContext = m_pDocument->GetScriptContext();
  if (!pScriptContext)
    return;

  CXFA_EventParam* pEventParam = pScriptContext->GetEventParam();
  if (!pEventParam)
    return;

  switch (dwFlag) {
    case XFA_Event::CancelAction:
      Script_EventPseudoModel_BooleanProperty(
          pValue, pEventParam->m_bCancelAction, bSetting);
      break;
    case XFA_Event::Change:
      Script_EventPseudoModel_StringProperty(pValue, pEventParam->m_wsChange,
                                             bSetting);
      break;
    case XFA_Event::CommitKey:
      Script_EventPseudoModel_InterProperty(pValue, pEventParam->m_iCommitKey,
                                            bSetting);
      break;
    case XFA_Event::FullText:
      Script_EventPseudoModel_StringProperty(pValue, pEventParam->m_wsFullText,
                                             bSetting);
      break;
    case XFA_Event::Keydown:
      Script_EventPseudoModel_BooleanProperty(pValue, pEventParam->m_bKeyDown,
                                              bSetting);
      break;
    case XFA_Event::Modifier:
      Script_EventPseudoModel_BooleanProperty(pValue, pEventParam->m_bModifier,
                                              bSetting);
      break;
    case XFA_Event::NewContentType:
      Script_EventPseudoModel_StringProperty(
          pValue, pEventParam->m_wsNewContentType, bSetting);
      break;
    case XFA_Event::NewText:
      Script_EventPseudoModel_StringProperty(pValue, pEventParam->m_wsNewText,
                                             bSetting);
      break;
    case XFA_Event::PreviousContentType:
      Script_EventPseudoModel_StringProperty(
          pValue, pEventParam->m_wsPrevContentType, bSetting);
      break;
    case XFA_Event::PreviousText:
      Script_EventPseudoModel_StringProperty(pValue, pEventParam->m_wsPrevText,
                                             bSetting);
      break;
    case XFA_Event::Reenter:
      Script_EventPseudoModel_BooleanProperty(pValue, pEventParam->m_bReenter,
                                              bSetting);
      break;
    case XFA_Event::SelectionEnd:
      Script_EventPseudoModel_InterProperty(pValue, pEventParam->m_iSelEnd,
                                            bSetting);
      break;
    case XFA_Event::SelectionStart:
      Script_EventPseudoModel_InterProperty(pValue, pEventParam->m_iSelStart,
                                            bSetting);
      break;
    case XFA_Event::Shift:
      Script_EventPseudoModel_BooleanProperty(pValue, pEventParam->m_bShift,
                                              bSetting);
      break;
    case XFA_Event::SoapFaultCode:
      Script_EventPseudoModel_StringProperty(
          pValue, pEventParam->m_wsSoapFaultCode, bSetting);
      break;
    case XFA_Event::SoapFaultString:
      Script_EventPseudoModel_StringProperty(
          pValue, pEventParam->m_wsSoapFaultString, bSetting);
      break;
    case XFA_Event::Target:
      break;
    default:
      break;
  }
}
void CScript_EventPseudoModel::Script_EventPseudoModel_Change(
    CFXJSE_Value* pValue,
    FX_BOOL bSetting,
    XFA_ATTRIBUTE eAttribute) {
  Script_EventPseudoModel_Property(pValue, XFA_Event::Change, bSetting);
}
void CScript_EventPseudoModel::Script_EventPseudoModel_CommitKey(
    CFXJSE_Value* pValue,
    FX_BOOL bSetting,
    XFA_ATTRIBUTE eAttribute) {
  Script_EventPseudoModel_Property(pValue, XFA_Event::CommitKey, bSetting);
}
void CScript_EventPseudoModel::Script_EventPseudoModel_FullText(
    CFXJSE_Value* pValue,
    FX_BOOL bSetting,
    XFA_ATTRIBUTE eAttribute) {
  Script_EventPseudoModel_Property(pValue, XFA_Event::FullText, bSetting);
}
void CScript_EventPseudoModel::Script_EventPseudoModel_KeyDown(
    CFXJSE_Value* pValue,
    FX_BOOL bSetting,
    XFA_ATTRIBUTE eAttribute) {
  Script_EventPseudoModel_Property(pValue, XFA_Event::Keydown, bSetting);
}
void CScript_EventPseudoModel::Script_EventPseudoModel_Modifier(
    CFXJSE_Value* pValue,
    FX_BOOL bSetting,
    XFA_ATTRIBUTE eAttribute) {
  Script_EventPseudoModel_Property(pValue, XFA_Event::Modifier, bSetting);
}
void CScript_EventPseudoModel::Script_EventPseudoModel_NewContentType(
    CFXJSE_Value* pValue,
    FX_BOOL bSetting,
    XFA_ATTRIBUTE eAttribute) {
  Script_EventPseudoModel_Property(pValue, XFA_Event::NewContentType, bSetting);
}
void CScript_EventPseudoModel::Script_EventPseudoModel_NewText(
    CFXJSE_Value* pValue,
    FX_BOOL bSetting,
    XFA_ATTRIBUTE eAttribute) {
  Script_EventPseudoModel_Property(pValue, XFA_Event::NewText, bSetting);
}
void CScript_EventPseudoModel::Script_EventPseudoModel_PrevContentType(
    CFXJSE_Value* pValue,
    FX_BOOL bSetting,
    XFA_ATTRIBUTE eAttribute) {
  Script_EventPseudoModel_Property(pValue, XFA_Event::PreviousContentType,
                                   bSetting);
}
void CScript_EventPseudoModel::Script_EventPseudoModel_PrevText(
    CFXJSE_Value* pValue,
    FX_BOOL bSetting,
    XFA_ATTRIBUTE eAttribute) {
  Script_EventPseudoModel_Property(pValue, XFA_Event::PreviousText, bSetting);
}
void CScript_EventPseudoModel::Script_EventPseudoModel_Reenter(
    CFXJSE_Value* pValue,
    FX_BOOL bSetting,
    XFA_ATTRIBUTE eAttribute) {
  Script_EventPseudoModel_Property(pValue, XFA_Event::Reenter, bSetting);
}
void CScript_EventPseudoModel::Script_EventPseudoModel_SelEnd(
    CFXJSE_Value* pValue,
    FX_BOOL bSetting,
    XFA_ATTRIBUTE eAttribute) {
  Script_EventPseudoModel_Property(pValue, XFA_Event::SelectionEnd, bSetting);
}
void CScript_EventPseudoModel::Script_EventPseudoModel_SelStart(
    CFXJSE_Value* pValue,
    FX_BOOL bSetting,
    XFA_ATTRIBUTE eAttribute) {
  Script_EventPseudoModel_Property(pValue, XFA_Event::SelectionStart, bSetting);
}
void CScript_EventPseudoModel::Script_EventPseudoModel_Shift(
    CFXJSE_Value* pValue,
    FX_BOOL bSetting,
    XFA_ATTRIBUTE eAttribute) {
  Script_EventPseudoModel_Property(pValue, XFA_Event::Shift, bSetting);
}
void CScript_EventPseudoModel::Script_EventPseudoModel_SoapFaultCode(
    CFXJSE_Value* pValue,
    FX_BOOL bSetting,
    XFA_ATTRIBUTE eAttribute) {
  Script_EventPseudoModel_Property(pValue, XFA_Event::SoapFaultCode, bSetting);
}
void CScript_EventPseudoModel::Script_EventPseudoModel_SoapFaultString(
    CFXJSE_Value* pValue,
    FX_BOOL bSetting,
    XFA_ATTRIBUTE eAttribute) {
  Script_EventPseudoModel_Property(pValue, XFA_Event::SoapFaultString,
                                   bSetting);
}
void CScript_EventPseudoModel::Script_EventPseudoModel_Target(
    CFXJSE_Value* pValue,
    FX_BOOL bSetting,
    XFA_ATTRIBUTE eAttribute) {
  Script_EventPseudoModel_Property(pValue, XFA_Event::Target, bSetting);
}
void CScript_EventPseudoModel::Script_EventPseudoModel_Emit(
    CFXJSE_Arguments* pArguments) {
  CXFA_ScriptContext* pScriptContext = m_pDocument->GetScriptContext();
  if (!pScriptContext) {
    return;
  }
  CXFA_EventParam* pEventParam = pScriptContext->GetEventParam();
  if (!pEventParam) {
    return;
  }
  CXFA_FFNotify* pNotify = m_pDocument->GetParser()->GetNotify();
  if (!pNotify) {
    return;
  }
  CXFA_FFWidgetHandler* pWidgetHandler = pNotify->GetWidgetHandler();
  if (!pWidgetHandler) {
    return;
  }
  pWidgetHandler->ProcessEvent(pEventParam->m_pTarget, pEventParam);
}
void CScript_EventPseudoModel::Script_EventPseudoModel_Reset(
    CFXJSE_Arguments* pArguments) {
  CXFA_ScriptContext* pScriptContext = m_pDocument->GetScriptContext();
  if (!pScriptContext) {
    return;
  }
  CXFA_EventParam* pEventParam = pScriptContext->GetEventParam();
  if (!pEventParam) {
    return;
  }
  pEventParam->Reset();
}
