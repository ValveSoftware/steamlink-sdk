// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fde/cfde_txtedtdorecord_insert.h"

#include "xfa/fde/cfde_txtedtengine.h"
#include "xfa/fwl/core/ifwl_edit.h"

CFDE_TxtEdtDoRecord_Insert::CFDE_TxtEdtDoRecord_Insert(
    CFDE_TxtEdtEngine* pEngine,
    int32_t nCaret,
    const FX_WCHAR* lpText,
    int32_t nLength)
    : m_pEngine(pEngine), m_nCaret(nCaret) {
  ASSERT(pEngine);
  FX_WCHAR* lpBuffer = m_wsInsert.GetBuffer(nLength);
  FXSYS_memcpy(lpBuffer, lpText, nLength * sizeof(FX_WCHAR));
  m_wsInsert.ReleaseBuffer();
}

CFDE_TxtEdtDoRecord_Insert::~CFDE_TxtEdtDoRecord_Insert() {}

bool CFDE_TxtEdtDoRecord_Insert::Undo() const {
  if (m_pEngine->IsSelect())
    m_pEngine->ClearSelection();

  m_pEngine->Inner_DeleteRange(m_nCaret, m_wsInsert.GetLength());
  FDE_TXTEDTPARAMS& Param = m_pEngine->m_Param;
  m_pEngine->m_ChangeInfo.nChangeType = FDE_TXTEDT_TEXTCHANGE_TYPE_Delete;
  m_pEngine->m_ChangeInfo.wsDelete = m_wsInsert;
  Param.pEventSink->On_TextChanged(m_pEngine, m_pEngine->m_ChangeInfo);
  m_pEngine->SetCaretPos(m_nCaret, true);
  return true;
}

bool CFDE_TxtEdtDoRecord_Insert::Redo() const {
  m_pEngine->Inner_Insert(m_nCaret, m_wsInsert.c_str(), m_wsInsert.GetLength());
  FDE_TXTEDTPARAMS& Param = m_pEngine->m_Param;
  m_pEngine->m_ChangeInfo.nChangeType = FDE_TXTEDT_TEXTCHANGE_TYPE_Insert;
  m_pEngine->m_ChangeInfo.wsDelete = m_wsInsert;
  Param.pEventSink->On_TextChanged(m_pEngine, m_pEngine->m_ChangeInfo);
  m_pEngine->SetCaretPos(m_nCaret, false);
  return true;
}
