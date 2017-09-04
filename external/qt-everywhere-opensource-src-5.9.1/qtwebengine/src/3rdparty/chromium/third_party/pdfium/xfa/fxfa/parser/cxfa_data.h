// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_CXFA_DATA_H_
#define XFA_FXFA_PARSER_CXFA_DATA_H_

#include "core/fxcrt/fx_system.h"
#include "core/fxge/fx_dib.h"
#include "xfa/fxfa/fxfa_basic.h"

class CXFA_Node;

class CXFA_Data {
 public:
  static FX_ARGB ToColor(const CFX_WideStringC& wsValue);

  explicit CXFA_Data(CXFA_Node* pNode) : m_pNode(pNode) {}

  operator bool() const { return !!m_pNode; }
  CXFA_Node* GetNode() const { return m_pNode; }
  XFA_Element GetElementType() const;

 protected:
  bool TryMeasure(XFA_ATTRIBUTE eAttr,
                  FX_FLOAT& fValue,
                  bool bUseDefault = false) const;
  bool SetMeasure(XFA_ATTRIBUTE eAttr, FX_FLOAT fValue);

  CXFA_Node* m_pNode;
};

#endif  // XFA_FXFA_PARSER_CXFA_DATA_H_
