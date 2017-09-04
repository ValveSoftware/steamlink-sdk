// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FDE_CFDE_TXTEDTDORECORD_DELETERANGE_H_
#define XFA_FDE_CFDE_TXTEDTDORECORD_DELETERANGE_H_

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "xfa/fde/ifde_txtedtdorecord.h"

class CFDE_TxtEdtEngine;

class CFDE_TxtEdtDoRecord_DeleteRange : public IFDE_TxtEdtDoRecord {
 public:
  CFDE_TxtEdtDoRecord_DeleteRange(CFDE_TxtEdtEngine* pEngine,
                                  int32_t nIndex,
                                  int32_t nCaret,
                                  const CFX_WideString& wsRange,
                                  bool bSel = false);
  ~CFDE_TxtEdtDoRecord_DeleteRange() override;

  bool Undo() const override;
  bool Redo() const override;

 private:
  CFDE_TxtEdtEngine* m_pEngine;
  bool m_bSel;
  int32_t m_nIndex;
  int32_t m_nCaret;
  CFX_WideString m_wsRange;
};

#endif  // XFA_FDE_CFDE_TXTEDTDORECORD_DELETERANGE_H_
