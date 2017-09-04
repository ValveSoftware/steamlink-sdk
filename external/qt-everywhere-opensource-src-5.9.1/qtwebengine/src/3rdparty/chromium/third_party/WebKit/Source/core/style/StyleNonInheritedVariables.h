// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleNonInheritedVariables_h
#define StyleNonInheritedVariables_h

#include "core/css/CSSValue.h"
#include "core/css/CSSVariableData.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class StyleNonInheritedVariables {
 public:
  static std::unique_ptr<StyleNonInheritedVariables> create() {
    return wrapUnique(new StyleNonInheritedVariables);
  }

  std::unique_ptr<StyleNonInheritedVariables> copy() {
    return wrapUnique(new StyleNonInheritedVariables(*this));
  }

  bool operator==(const StyleNonInheritedVariables& other) const;
  bool operator!=(const StyleNonInheritedVariables& other) const {
    return !(*this == other);
  }

  void setVariable(const AtomicString& name,
                   PassRefPtr<CSSVariableData> value) {
    m_data.set(name, std::move(value));
  }
  CSSVariableData* getVariable(const AtomicString& name) const;
  void removeVariable(const AtomicString&);

  void setRegisteredVariable(const AtomicString&, const CSSValue*);
  CSSValue* registeredVariable(const AtomicString& name) const {
    return m_registeredData.get(name);
  }

 private:
  StyleNonInheritedVariables() = default;
  StyleNonInheritedVariables(StyleNonInheritedVariables&);

  friend class CSSVariableResolver;

  HashMap<AtomicString, RefPtr<CSSVariableData>> m_data;
  HashMap<AtomicString, Persistent<CSSValue>> m_registeredData;
};

}  // namespace blink

#endif  // StyleNonInheritedVariables_h
