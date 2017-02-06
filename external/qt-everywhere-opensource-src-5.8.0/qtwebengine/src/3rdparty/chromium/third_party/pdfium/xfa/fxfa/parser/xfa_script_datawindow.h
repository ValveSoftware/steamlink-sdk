// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_XFA_SCRIPT_DATAWINDOW_H_
#define XFA_FXFA_PARSER_XFA_SCRIPT_DATAWINDOW_H_

#include "fxjse/include/cfxjse_arguments.h"
#include "xfa/fxfa/parser/xfa_object.h"

class CScript_DataWindow : public CXFA_Object {
 public:
  CScript_DataWindow(CXFA_Document* pDocument);
  ~CScript_DataWindow() override;

  void Script_DataWindow_MoveCurrentRecord(CFXJSE_Arguments* pArguments);
  void Script_DataWindow_Record(CFXJSE_Arguments* pArguments);
  void Script_DataWindow_GotoRecord(CFXJSE_Arguments* pArguments);
  void Script_DataWindow_IsRecordGroup(CFXJSE_Arguments* pArguments);
  void Script_DataWindow_RecordsBefore(CFXJSE_Value* pValue,
                                       FX_BOOL bSetting,
                                       XFA_ATTRIBUTE eAttribute);
  void Script_DataWindow_CurrentRecordNumber(CFXJSE_Value* pValue,
                                             FX_BOOL bSetting,
                                             XFA_ATTRIBUTE eAttribute);
  void Script_DataWindow_RecordsAfter(CFXJSE_Value* pValue,
                                      FX_BOOL bSetting,
                                      XFA_ATTRIBUTE eAttribute);
  void Script_DataWindow_IsDefined(CFXJSE_Value* pValue,
                                   FX_BOOL bSetting,
                                   XFA_ATTRIBUTE eAttribute);
};

#endif  // XFA_FXFA_PARSER_XFA_SCRIPT_DATAWINDOW_H_
