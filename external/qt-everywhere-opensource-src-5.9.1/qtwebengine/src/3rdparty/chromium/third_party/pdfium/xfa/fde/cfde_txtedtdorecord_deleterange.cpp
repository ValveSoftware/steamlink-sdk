// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fde/cfde_txtedtdorecord_deleterange.h"

#include "xfa/fde/cfde_txtedtengine.h"
#include "xfa/fwl/core/ifwl_edit.h"

CFDE_TxtEdtDoRecord_DeleteRange::CFDE_TxtEdtDoRecord_DeleteRange(
    CFDE_TxtEdtEngine* pEngine,
    int32_t nIndex,
    int32_t nCaret,
    const CFX_WideString& wsRange,
    bool bSel)
    : m_pEngine(pEngine),
      m_bSel(bSel),
      m_nIndex(nIndex),
      m_nCaret(nCaret),
      m_wsRange(wsRange) {
  ASSERT(pEngine);
}

CFDE_TxtEdtDoRecord_DeleteRange::~CFDE_TxtEdtDoRecord_DeleteRange() {}

bool CFDE_TxtEdtDoRecord_DeleteRange::Undo() const {
  if (m_pEngine->IsSelect())
    m_pEngine->ClearSelection();

  m_pEngine->Inner_Insert(m_nIndex, m_wsRange.c_str(), m_wsRange.GetLength());
  if (m_bSel)
    m_pEngine->AddSelRange(m_nIndex, m_wsRange.GetLength());

  FDE_TXTEDTPARAMS& Param = m_pEngine->m_Param;
  m_pEngine->m_ChangeInfo.nChangeType = FDE_TXTEDT_TEXTCHANGE_TYPE_Insert;
  m_pEngine->m_ChangeInfo.wsDelete = m_wsRange;
  Param.pEventSink->On_TextChanged(m_pEngine, m_pEngine->m_ChangeInfo);
  m_pEngine->SetCaretPos(m_nCaret, true);
  return true;
}

bool CFDE_TxtEdtDoRecord_DeleteRange::Redo() const {
  m_pEngine->Inner_DeleteRange(m_nIndex, m_wsRange.GetLength());
  if (m_bSel)
    m_pEngine->RemoveSelRange(m_nIndex, m_wsRange.GetLength());

  FDE_TXTEDTPARAMS& Param = m_pEngine->m_Param;
  m_pEngine->m_ChangeInfo.nChangeType = FDE_TXTEDT_TEXTCHANGE_TYPE_Insert;
  m_pEngine->m_ChangeInfo.wsDelete = m_wsRange;
  Param.pEventSink->On_TextChanged(m_pEngine, m_pEngine->m_ChangeInfo);
  m_pEngine->SetCaretPos(m_nIndex, true);
  return true;
}
