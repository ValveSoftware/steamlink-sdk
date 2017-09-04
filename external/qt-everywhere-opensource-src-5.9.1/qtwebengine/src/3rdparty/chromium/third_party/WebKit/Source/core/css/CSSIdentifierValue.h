// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSIdentifierValue_h
#define CSSIdentifierValue_h

#include "core/CSSValueKeywords.h"
#include "core/css/CSSValue.h"
#include "wtf/TypeTraits.h"

namespace blink {

// CSSIdentifierValue stores CSS value keywords, e.g. 'none', 'auto',
// 'lower-roman'.
// TODO(sashab): Rename this class to CSSKeywordValue once it no longer
// conflicts with CSSOM's CSSKeywordValue class.
class CORE_EXPORT CSSIdentifierValue : public CSSValue {
 public:
  static CSSIdentifierValue* create(CSSValueID);

  // TODO(sashab): Rename this to createFromPlatformValue().
  template <typename T>
  static CSSIdentifierValue* create(T value) {
    static_assert(!std::is_same<T, CSSValueID>::value,
                  "Do not call create() with a CSSValueID; call "
                  "createIdentifier() instead");
    return new CSSIdentifierValue(value);
  }

  static CSSIdentifierValue* create(const Length& value) {
    return new CSSIdentifierValue(value);
  }

  CSSValueID getValueID() const { return m_valueID; }

  String customCSSText() const;

  bool equals(const CSSIdentifierValue& other) const {
    return m_valueID == other.m_valueID;
  }

  template <typename T>
  inline T convertTo() const;  // Defined in CSSPrimitiveValueMappings.h

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  explicit CSSIdentifierValue(CSSValueID);

  // TODO(sashab): Remove this function, and update mapping methods to
  // specialize the create() method instead.
  template <typename T>
  CSSIdentifierValue(T);  // Defined in CSSPrimitiveValueMappings.h

  CSSIdentifierValue(const Length&);

  CSSValueID m_valueID;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSIdentifierValue, isIdentifierValue());

}  // namespace blink

#endif  // CSSIdentifierValue_h
