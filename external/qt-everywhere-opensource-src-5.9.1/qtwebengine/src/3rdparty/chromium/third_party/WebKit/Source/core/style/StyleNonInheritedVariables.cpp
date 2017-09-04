// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/StyleNonInheritedVariables.h"

#include "core/style/DataEquivalency.h"

namespace blink {

bool StyleNonInheritedVariables::operator==(
    const StyleNonInheritedVariables& other) const {
  if (m_data.size() != other.m_data.size())
    return false;

  for (const auto& iter : m_data) {
    RefPtr<CSSVariableData> otherData = other.m_data.get(iter.key);
    if (!dataEquivalent(iter.value, otherData))
      return false;
  }

  return true;
}

CSSVariableData* StyleNonInheritedVariables::getVariable(
    const AtomicString& name) const {
  return m_data.get(name);
}

void StyleNonInheritedVariables::setRegisteredVariable(
    const AtomicString& name,
    const CSSValue* parsedValue) {
  m_registeredData.set(name, const_cast<CSSValue*>(parsedValue));
}

void StyleNonInheritedVariables::removeVariable(const AtomicString& name) {
  m_data.set(name, nullptr);
  m_registeredData.set(name, nullptr);
}

StyleNonInheritedVariables::StyleNonInheritedVariables(
    StyleNonInheritedVariables& other) {
  m_data = other.m_data;
  m_registeredData = other.m_registeredData;
}

}  // namespace blink
