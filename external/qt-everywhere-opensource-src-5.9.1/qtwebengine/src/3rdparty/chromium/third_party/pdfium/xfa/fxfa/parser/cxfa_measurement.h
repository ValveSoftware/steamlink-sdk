// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_CXFA_MEASUREMENT_H_
#define XFA_FXFA_PARSER_CXFA_MEASUREMENT_H_

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "xfa/fxfa/fxfa_basic.h"

class CXFA_Measurement {
 public:
  explicit CXFA_Measurement(const CFX_WideStringC& wsMeasure);
  CXFA_Measurement();
  CXFA_Measurement(FX_FLOAT fValue, XFA_UNIT eUnit);

  void Set(const CFX_WideStringC& wsMeasure);
  void Set(FX_FLOAT fValue, XFA_UNIT eUnit) {
    m_fValue = fValue;
    m_eUnit = eUnit;
  }

  XFA_UNIT GetUnit(const CFX_WideStringC& wsUnit);
  XFA_UNIT GetUnit() const { return m_eUnit; }
  FX_FLOAT GetValue() const { return m_fValue; }

  bool ToString(CFX_WideString& wsMeasure) const;
  bool ToUnit(XFA_UNIT eUnit, FX_FLOAT& fValue) const;
  FX_FLOAT ToUnit(XFA_UNIT eUnit) const {
    FX_FLOAT f;
    return ToUnit(eUnit, f) ? f : 0;
  }

 private:
  FX_FLOAT m_fValue;
  XFA_UNIT m_eUnit;
};

#endif  // XFA_FXFA_PARSER_CXFA_MEASUREMENT_H_
