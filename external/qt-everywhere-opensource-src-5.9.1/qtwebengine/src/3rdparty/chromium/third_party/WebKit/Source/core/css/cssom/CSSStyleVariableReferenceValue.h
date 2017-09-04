// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSStyleVariableReferenceValue_h
#define CSSStyleVariableReferenceValue_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/css/cssom/CSSUnparsedValue.h"

namespace blink {

class CORE_EXPORT CSSStyleVariableReferenceValue final
    : public GarbageCollectedFinalized<CSSStyleVariableReferenceValue>,
      public ScriptWrappable {
  WTF_MAKE_NONCOPYABLE(CSSStyleVariableReferenceValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  virtual ~CSSStyleVariableReferenceValue() {}

  static CSSStyleVariableReferenceValue* create(
      const String& variable,
      const CSSUnparsedValue* fallback) {
    return new CSSStyleVariableReferenceValue(variable, fallback);
  }

  const String& variable() const { return m_variable; }

  CSSUnparsedValue* fallback() {
    return const_cast<CSSUnparsedValue*>(m_fallback.get());
  }

  DEFINE_INLINE_TRACE() { visitor->trace(m_fallback); }

 protected:
  CSSStyleVariableReferenceValue(const String& variable,
                                 const CSSUnparsedValue* fallback)
      : m_variable(variable), m_fallback(fallback) {}

  String m_variable;
  Member<const CSSUnparsedValue> m_fallback;
};

}  // namespace blink

#endif  // CSSStyleVariableReference_h
