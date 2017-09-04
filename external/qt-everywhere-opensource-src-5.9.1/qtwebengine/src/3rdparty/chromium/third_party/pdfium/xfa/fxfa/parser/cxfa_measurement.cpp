// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_measurement.h"

#include "core/fxcrt/fx_ext.h"

CXFA_Measurement::CXFA_Measurement(const CFX_WideStringC& wsMeasure) {
  Set(wsMeasure);
}

CXFA_Measurement::CXFA_Measurement() {
  Set(-1, XFA_UNIT_Unknown);
}

CXFA_Measurement::CXFA_Measurement(FX_FLOAT fValue, XFA_UNIT eUnit) {
  Set(fValue, eUnit);
}

void CXFA_Measurement::Set(const CFX_WideStringC& wsMeasure) {
  if (wsMeasure.IsEmpty()) {
    m_fValue = 0;
    m_eUnit = XFA_UNIT_Unknown;
    return;
  }
  int32_t iUsedLen = 0;
  int32_t iOffset = (wsMeasure.GetAt(0) == L'=') ? 1 : 0;
  FX_FLOAT fValue = FXSYS_wcstof(wsMeasure.c_str() + iOffset,
                                 wsMeasure.GetLength() - iOffset, &iUsedLen);
  XFA_UNIT eUnit = GetUnit(wsMeasure.Mid(iOffset + iUsedLen));
  Set(fValue, eUnit);
}

bool CXFA_Measurement::ToString(CFX_WideString& wsMeasure) const {
  switch (GetUnit()) {
    case XFA_UNIT_Mm:
      wsMeasure.Format(L"%.8gmm", GetValue());
      return true;
    case XFA_UNIT_Pt:
      wsMeasure.Format(L"%.8gpt", GetValue());
      return true;
    case XFA_UNIT_In:
      wsMeasure.Format(L"%.8gin", GetValue());
      return true;
    case XFA_UNIT_Cm:
      wsMeasure.Format(L"%.8gcm", GetValue());
      return true;
    case XFA_UNIT_Mp:
      wsMeasure.Format(L"%.8gmp", GetValue());
      return true;
    case XFA_UNIT_Pc:
      wsMeasure.Format(L"%.8gpc", GetValue());
      return true;
    case XFA_UNIT_Em:
      wsMeasure.Format(L"%.8gem", GetValue());
      return true;
    case XFA_UNIT_Percent:
      wsMeasure.Format(L"%.8g%%", GetValue());
      return true;
    default:
      wsMeasure.Format(L"%.8g", GetValue());
      return false;
  }
}

bool CXFA_Measurement::ToUnit(XFA_UNIT eUnit, FX_FLOAT& fValue) const {
  fValue = GetValue();
  XFA_UNIT eFrom = GetUnit();
  if (eFrom == eUnit)
    return true;

  switch (eFrom) {
    case XFA_UNIT_Pt:
      break;
    case XFA_UNIT_Mm:
      fValue *= 72 / 2.54f / 10;
      break;
    case XFA_UNIT_In:
      fValue *= 72;
      break;
    case XFA_UNIT_Cm:
      fValue *= 72 / 2.54f;
      break;
    case XFA_UNIT_Mp:
      fValue *= 0.001f;
      break;
    case XFA_UNIT_Pc:
      fValue *= 12.0f;
      break;
    default:
      fValue = 0;
      return false;
  }
  switch (eUnit) {
    case XFA_UNIT_Pt:
      return true;
    case XFA_UNIT_Mm:
      fValue /= 72 / 2.54f / 10;
      return true;
    case XFA_UNIT_In:
      fValue /= 72;
      return true;
    case XFA_UNIT_Cm:
      fValue /= 72 / 2.54f;
      return true;
    case XFA_UNIT_Mp:
      fValue /= 0.001f;
      return true;
    case XFA_UNIT_Pc:
      fValue /= 12.0f;
      return true;
    default:
      fValue = 0;
      return false;
  }
}

XFA_UNIT CXFA_Measurement::GetUnit(const CFX_WideStringC& wsUnit) {
  if (wsUnit == FX_WSTRC(L"mm"))
    return XFA_UNIT_Mm;
  if (wsUnit == FX_WSTRC(L"pt"))
    return XFA_UNIT_Pt;
  if (wsUnit == FX_WSTRC(L"in"))
    return XFA_UNIT_In;
  if (wsUnit == FX_WSTRC(L"cm"))
    return XFA_UNIT_Cm;
  if (wsUnit == FX_WSTRC(L"pc"))
    return XFA_UNIT_Pc;
  if (wsUnit == FX_WSTRC(L"mp"))
    return XFA_UNIT_Mp;
  if (wsUnit == FX_WSTRC(L"em"))
    return XFA_UNIT_Em;
  if (wsUnit == FX_WSTRC(L"%"))
    return XFA_UNIT_Percent;
  return XFA_UNIT_Unknown;
}
