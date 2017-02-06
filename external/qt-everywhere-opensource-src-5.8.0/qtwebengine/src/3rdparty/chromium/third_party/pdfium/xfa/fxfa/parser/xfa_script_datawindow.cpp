// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/xfa_script_datawindow.h"

#include "fxjse/include/cfxjse_arguments.h"
#include "xfa/fxfa/parser/xfa_doclayout.h"
#include "xfa/fxfa/parser/xfa_document.h"
#include "xfa/fxfa/parser/xfa_localemgr.h"
#include "xfa/fxfa/parser/xfa_object.h"
#include "xfa/fxfa/parser/xfa_parser.h"
#include "xfa/fxfa/parser/xfa_script.h"
#include "xfa/fxfa/parser/xfa_utils.h"

CScript_DataWindow::CScript_DataWindow(CXFA_Document* pDocument)
    : CXFA_Object(pDocument, XFA_ObjectType::Object, XFA_Element::DataWindow) {}

CScript_DataWindow::~CScript_DataWindow() {}

void CScript_DataWindow::Script_DataWindow_MoveCurrentRecord(
    CFXJSE_Arguments* pArguments) {}

void CScript_DataWindow::Script_DataWindow_Record(
    CFXJSE_Arguments* pArguments) {}

void CScript_DataWindow::Script_DataWindow_GotoRecord(
    CFXJSE_Arguments* pArguments) {}

void CScript_DataWindow::Script_DataWindow_IsRecordGroup(
    CFXJSE_Arguments* pArguments) {}

void CScript_DataWindow::Script_DataWindow_RecordsBefore(
    CFXJSE_Value* pValue,
    FX_BOOL bSetting,
    XFA_ATTRIBUTE eAttribute) {}

void CScript_DataWindow::Script_DataWindow_CurrentRecordNumber(
    CFXJSE_Value* pValue,
    FX_BOOL bSetting,
    XFA_ATTRIBUTE eAttribute) {}

void CScript_DataWindow::Script_DataWindow_RecordsAfter(
    CFXJSE_Value* pValue,
    FX_BOOL bSetting,
    XFA_ATTRIBUTE eAttribute) {}

void CScript_DataWindow::Script_DataWindow_IsDefined(CFXJSE_Value* pValue,
                                                     FX_BOOL bSetting,
                                                     XFA_ATTRIBUTE eAttribute) {
}
