// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSColorValue_h
#define CSSColorValue_h

#include "core/css/CSSValue.h"
#include "platform/graphics/Color.h"
#include "wtf/PassRefPtr.h"

namespace blink {

// Represents the non-keyword subset of <color>.
class CSSColorValue : public CSSValue {
 public:
  // TODO(sashab): Make this create() method take a Color instead.
  static CSSColorValue* create(RGBA32 color);

  String customCSSText() const {
    return m_color.serializedAsCSSComponentValue();
  }

  Color value() const { return m_color; }

  bool equals(const CSSColorValue& other) const {
    return m_color == other.m_color;
  }

  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {
    CSSValue::traceAfterDispatch(visitor);
  }

 private:
  friend class CSSValuePool;

  CSSColorValue(Color color) : CSSValue(ColorClass), m_color(color) {}

  Color m_color;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSColorValue, isColorValue());

}  // namespace blink

#endif  // CSSColorValue_h
