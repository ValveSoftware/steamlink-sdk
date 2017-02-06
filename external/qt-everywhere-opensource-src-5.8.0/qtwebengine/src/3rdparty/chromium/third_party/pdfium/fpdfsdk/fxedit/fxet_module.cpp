// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/include/cpdf_variabletext.h"
#include "fpdfsdk/fxedit/include/fxet_edit.h"
#include "fpdfsdk/fxedit/include/fxet_list.h"

IFX_Edit* IFX_Edit::NewEdit() {
  return new CFX_Edit(new CPDF_VariableText());
}

void IFX_Edit::DelEdit(IFX_Edit* pEdit) {
  delete pEdit->GetVariableText();
  delete static_cast<CFX_Edit*>(pEdit);
}

IFX_List* IFX_List::NewList() {
  return new CFX_ListCtrl();
}

void IFX_List::DelList(IFX_List* pList) {
  ASSERT(pList);
  delete (CFX_ListCtrl*)pList;
}
