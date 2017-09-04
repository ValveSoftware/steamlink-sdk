// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8HTMLConstructor_h
#define V8HTMLConstructor_h

#include "bindings/core/v8/WrapperTypeInfo.h"
#include "core/HTMLElementTypeHelpers.h"
#include <v8.h>

namespace blink {

// https://html.spec.whatwg.org/multipage/dom.html#html-element-constructors
class CORE_EXPORT V8HTMLConstructor {
  STATIC_ONLY(V8HTMLConstructor)

 public:
  static void htmlConstructor(const v8::FunctionCallbackInfo<v8::Value>&,
                              const WrapperTypeInfo&,
                              const HTMLElementType);
};

}  // namespace blink

#endif  // V8HTMLConstructor_h
