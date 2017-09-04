// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleInheritedVariables_h
#define StyleInheritedVariables_h

#include "core/css/CSSValue.h"
#include "core/css/CSSVariableData.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/RefCounted.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class StyleInheritedVariables : public RefCounted<StyleInheritedVariables> {
 public:
  static PassRefPtr<StyleInheritedVariables> create() {
    return adoptRef(new StyleInheritedVariables());
  }

  PassRefPtr<StyleInheritedVariables> copy() {
    return adoptRef(new StyleInheritedVariables(*this));
  }

  bool operator==(const StyleInheritedVariables& other) const;
  bool operator!=(const StyleInheritedVariables& other) const {
    return !(*this == other);
  }

  void setVariable(const AtomicString& name,
                   PassRefPtr<CSSVariableData> value) {
    m_data.set(name, std::move(value));
  }
  CSSVariableData* getVariable(const AtomicString& name) const;
  void removeVariable(const AtomicString&);

  void setRegisteredVariable(const AtomicString&, const CSSValue*);
  CSSValue* registeredVariable(const AtomicString&) const;

  // This map will contain null pointers if variables are invalid due to
  // cycles or referencing invalid variables without using a fallback.
  // Note that this method is slow as a new map is constructed.
  std::unique_ptr<HashMap<AtomicString, RefPtr<CSSVariableData>>> getVariables()
      const;

 private:
  StyleInheritedVariables() : m_root(nullptr) {}
  StyleInheritedVariables(StyleInheritedVariables& other);

  friend class CSSVariableResolver;

  HashMap<AtomicString, RefPtr<CSSVariableData>> m_data;
  HashMap<AtomicString, Persistent<CSSValue>> m_registeredData;
  RefPtr<StyleInheritedVariables> m_root;
};

}  // namespace blink

#endif  // StyleInheritedVariables_h
