// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_XFA_SCRIPT_EVENTPSEUDOMODEL_H_
#define XFA_FXFA_PARSER_XFA_SCRIPT_EVENTPSEUDOMODEL_H_

#include "fxjse/include/cfxjse_arguments.h"
#include "xfa/fxfa/parser/xfa_object.h"

enum class XFA_Event {
  Change = 0,
  CommitKey,
  FullText,
  Keydown,
  Modifier,
  NewContentType,
  NewText,
  PreviousContentType,
  PreviousText,
  Reenter,
  SelectionEnd,
  SelectionStart,
  Shift,
  SoapFaultCode,
  SoapFaultString,
  Target,
  CancelAction
};

class CScript_EventPseudoModel : public CXFA_Object {
 public:
  explicit CScript_EventPseudoModel(CXFA_Document* pDocument);
  ~CScript_EventPseudoModel() override;

  void Script_EventPseudoModel_Change(CFXJSE_Value* pValue,
                                      FX_BOOL bSetting,
                                      XFA_ATTRIBUTE eAttribute);
  void Script_EventPseudoModel_CommitKey(CFXJSE_Value* pValue,
                                         FX_BOOL bSetting,
                                         XFA_ATTRIBUTE eAttribute);
  void Script_EventPseudoModel_FullText(CFXJSE_Value* pValue,
                                        FX_BOOL bSetting,
                                        XFA_ATTRIBUTE eAttribute);
  void Script_EventPseudoModel_KeyDown(CFXJSE_Value* pValue,
                                       FX_BOOL bSetting,
                                       XFA_ATTRIBUTE eAttribute);
  void Script_EventPseudoModel_Modifier(CFXJSE_Value* pValue,
                                        FX_BOOL bSetting,
                                        XFA_ATTRIBUTE eAttribute);
  void Script_EventPseudoModel_NewContentType(CFXJSE_Value* pValue,
                                              FX_BOOL bSetting,
                                              XFA_ATTRIBUTE eAttribute);
  void Script_EventPseudoModel_NewText(CFXJSE_Value* pValue,
                                       FX_BOOL bSetting,
                                       XFA_ATTRIBUTE eAttribute);
  void Script_EventPseudoModel_PrevContentType(CFXJSE_Value* pValue,
                                               FX_BOOL bSetting,
                                               XFA_ATTRIBUTE eAttribute);
  void Script_EventPseudoModel_PrevText(CFXJSE_Value* pValue,
                                        FX_BOOL bSetting,
                                        XFA_ATTRIBUTE eAttribute);
  void Script_EventPseudoModel_Reenter(CFXJSE_Value* pValue,
                                       FX_BOOL bSetting,
                                       XFA_ATTRIBUTE eAttribute);
  void Script_EventPseudoModel_SelEnd(CFXJSE_Value* pValue,
                                      FX_BOOL bSetting,
                                      XFA_ATTRIBUTE eAttribute);
  void Script_EventPseudoModel_SelStart(CFXJSE_Value* pValue,
                                        FX_BOOL bSetting,
                                        XFA_ATTRIBUTE eAttribute);
  void Script_EventPseudoModel_Shift(CFXJSE_Value* pValue,
                                     FX_BOOL bSetting,
                                     XFA_ATTRIBUTE eAttribute);
  void Script_EventPseudoModel_SoapFaultCode(CFXJSE_Value* pValue,
                                             FX_BOOL bSetting,
                                             XFA_ATTRIBUTE eAttribute);
  void Script_EventPseudoModel_SoapFaultString(CFXJSE_Value* pValue,
                                               FX_BOOL bSetting,
                                               XFA_ATTRIBUTE eAttribute);
  void Script_EventPseudoModel_Target(CFXJSE_Value* pValue,
                                      FX_BOOL bSetting,
                                      XFA_ATTRIBUTE eAttribute);

  void Script_EventPseudoModel_Emit(CFXJSE_Arguments* pArguments);
  void Script_EventPseudoModel_Reset(CFXJSE_Arguments* pArguments);

 protected:
  void Script_EventPseudoModel_Property(CFXJSE_Value* pValue,
                                        XFA_Event dwFlag,
                                        FX_BOOL bSetting);
};

#endif  // XFA_FXFA_PARSER_XFA_SCRIPT_EVENTPSEUDOMODEL_H_
