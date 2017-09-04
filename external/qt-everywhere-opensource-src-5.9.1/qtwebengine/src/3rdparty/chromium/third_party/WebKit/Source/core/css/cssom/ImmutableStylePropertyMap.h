// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImmutableStylePropertyMap_h
#define ImmutableStylePropertyMap_h

#include "core/css/cssom/StylePropertyMap.h"

namespace blink {

class CORE_EXPORT ImmutableStylePropertyMap : public StylePropertyMap {
  WTF_MAKE_NONCOPYABLE(ImmutableStylePropertyMap);

 public:
  void set(CSSPropertyID,
           CSSStyleValueOrCSSStyleValueSequenceOrString&,
           ExceptionState& exceptionState) override {
    exceptionState.throwTypeError("This StylePropertyMap is immutable.");
  }

  void append(CSSPropertyID,
              CSSStyleValueOrCSSStyleValueSequenceOrString&,
              ExceptionState& exceptionState) override {
    exceptionState.throwTypeError("This StylePropertyMap is immutable.");
  }

  void remove(CSSPropertyID, ExceptionState& exceptionState) override {
    exceptionState.throwTypeError("This StylePropertyMap is immutable.");
  }

 protected:
  ImmutableStylePropertyMap() = default;
};

}  // namespace blink

#endif
