// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FDE_CFX_WORDBREAK_H_
#define XFA_FDE_CFX_WORDBREAK_H_

#include <memory>

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"

class IFX_CharIter;

class CFX_WordBreak {
 public:
  CFX_WordBreak();
  ~CFX_WordBreak();

  void Attach(IFX_CharIter* pIter);
  void Attach(const CFX_WideString& wsText);
  bool Next(bool bPrev);
  void SetAt(int32_t nIndex);
  int32_t GetWordPos() const;
  int32_t GetWordLength() const;
  void GetWord(CFX_WideString& wsWord) const;
  bool IsEOF(bool bTail) const;

 protected:
  bool FindNextBreakPos(IFX_CharIter* pIter, bool bPrev, bool bFromNext = true);

 private:
  std::unique_ptr<IFX_CharIter> m_pPreIter;
  std::unique_ptr<IFX_CharIter> m_pCurIter;
};

#endif  // XFA_FDE_CFX_WORDBREAK_H_
