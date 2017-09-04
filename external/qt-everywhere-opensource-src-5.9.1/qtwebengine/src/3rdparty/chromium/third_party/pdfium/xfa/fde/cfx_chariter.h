// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FDE_CFX_CHARITER_H_
#define XFA_FDE_CFX_CHARITER_H_

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "xfa/fde/ifx_chariter.h"

class CFX_CharIter : public IFX_CharIter {
 public:
  explicit CFX_CharIter(const CFX_WideString& wsText);
  ~CFX_CharIter() override;

  bool Next(bool bPrev = false) override;
  FX_WCHAR GetChar() override;
  void SetAt(int32_t nIndex) override;
  int32_t GetAt() const override;
  bool IsEOF(bool bTail = true) const override;
  IFX_CharIter* Clone() override;

 private:
  const CFX_WideString& m_wsText;
  int32_t m_nIndex;
};

#endif  // XFA_FDE_CFX_CHARITER_H_
