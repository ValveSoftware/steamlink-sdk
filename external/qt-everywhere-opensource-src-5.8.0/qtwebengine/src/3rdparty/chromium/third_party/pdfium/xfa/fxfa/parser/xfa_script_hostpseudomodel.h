// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_XFA_SCRIPT_HOSTPSEUDOMODEL_H_
#define XFA_FXFA_PARSER_XFA_SCRIPT_HOSTPSEUDOMODEL_H_

#include "fxjse/include/cfxjse_arguments.h"
#include "xfa/fxfa/parser/xfa_document.h"
#include "xfa/fxfa/parser/xfa_object.h"

class CScript_HostPseudoModel : public CXFA_Object {
 public:
  CScript_HostPseudoModel(CXFA_Document* pDocument);
  ~CScript_HostPseudoModel() override;

  void Script_HostPseudoModel_AppType(CFXJSE_Value* pValue,
                                      FX_BOOL bSetting,
                                      XFA_ATTRIBUTE eAttribute);
  void Script_HostPseudoModel_FoxitAppType(CFXJSE_Value* pValue,
                                           FX_BOOL bSetting,
                                           XFA_ATTRIBUTE eAttribute);
  void Script_HostPseudoModel_CalculationsEnabled(CFXJSE_Value* pValue,
                                                  FX_BOOL bSetting,
                                                  XFA_ATTRIBUTE eAttribute);
  void Script_HostPseudoModel_CurrentPage(CFXJSE_Value* pValue,
                                          FX_BOOL bSetting,
                                          XFA_ATTRIBUTE eAttribute);
  void Script_HostPseudoModel_Language(CFXJSE_Value* pValue,
                                       FX_BOOL bSetting,
                                       XFA_ATTRIBUTE eAttribute);
  void Script_HostPseudoModel_NumPages(CFXJSE_Value* pValue,
                                       FX_BOOL bSetting,
                                       XFA_ATTRIBUTE eAttribute);
  void Script_HostPseudoModel_Platform(CFXJSE_Value* pValue,
                                       FX_BOOL bSetting,
                                       XFA_ATTRIBUTE eAttribute);
  void Script_HostPseudoModel_Title(CFXJSE_Value* pValue,
                                    FX_BOOL bSetting,
                                    XFA_ATTRIBUTE eAttribute);
  void Script_HostPseudoModel_ValidationsEnabled(CFXJSE_Value* pValue,
                                                 FX_BOOL bSetting,
                                                 XFA_ATTRIBUTE eAttribute);
  void Script_HostPseudoModel_Variation(CFXJSE_Value* pValue,
                                        FX_BOOL bSetting,
                                        XFA_ATTRIBUTE eAttribute);
  void Script_HostPseudoModel_Version(CFXJSE_Value* pValue,
                                      FX_BOOL bSetting,
                                      XFA_ATTRIBUTE eAttribute);
  void Script_HostPseudoModel_FoxitVersion(CFXJSE_Value* pValue,
                                           FX_BOOL bSetting,
                                           XFA_ATTRIBUTE eAttribute);
  void Script_HostPseudoModel_Name(CFXJSE_Value* pValue,
                                   FX_BOOL bSetting,
                                   XFA_ATTRIBUTE eAttribute);
  void Script_HostPseudoModel_FoxitName(CFXJSE_Value* pValue,
                                        FX_BOOL bSetting,
                                        XFA_ATTRIBUTE eAttribute);

  void Script_HostPseudoModel_GotoURL(CFXJSE_Arguments* pArguments);
  void Script_HostPseudoModel_OpenList(CFXJSE_Arguments* pArguments);
  void Script_HostPseudoModel_Response(CFXJSE_Arguments* pArguments);
  void Script_HostPseudoModel_DocumentInBatch(CFXJSE_Arguments* pArguments);
  void Script_HostPseudoModel_ResetData(CFXJSE_Arguments* pArguments);
  void Script_HostPseudoModel_Beep(CFXJSE_Arguments* pArguments);
  void Script_HostPseudoModel_SetFocus(CFXJSE_Arguments* pArguments);
  void Script_HostPseudoModel_GetFocus(CFXJSE_Arguments* pArguments);
  void Script_HostPseudoModel_MessageBox(CFXJSE_Arguments* pArguments);
  void Script_HostPseudoModel_DocumentCountInBatch(
      CFXJSE_Arguments* pArguments);
  void Script_HostPseudoModel_Print(CFXJSE_Arguments* pArguments);
  void Script_HostPseudoModel_ImportData(CFXJSE_Arguments* pArguments);
  void Script_HostPseudoModel_ExportData(CFXJSE_Arguments* pArguments);
  void Script_HostPseudoModel_PageUp(CFXJSE_Arguments* pArguments);
  void Script_HostPseudoModel_PageDown(CFXJSE_Arguments* pArguments);
  void Script_HostPseudoModel_CurrentDateTime(CFXJSE_Arguments* pArguments);

 protected:
  void Script_HostPseudoModel_LoadString(CFXJSE_Value* pValue,
                                         CXFA_FFNotify* pNotify,
                                         uint32_t dwFlag);
  FX_BOOL Script_HostPseudoModel_ValidateArgsForMsg(
      CFXJSE_Arguments* pArguments,
      int32_t iArgIndex,
      CFX_WideString& wsValue);
};

#endif  // XFA_FXFA_PARSER_XFA_SCRIPT_HOSTPSEUDOMODEL_H_
