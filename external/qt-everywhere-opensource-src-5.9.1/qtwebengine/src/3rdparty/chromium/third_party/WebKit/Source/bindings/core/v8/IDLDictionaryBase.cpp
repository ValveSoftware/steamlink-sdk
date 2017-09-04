// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/IDLDictionaryBase.h"

namespace blink {

v8::Local<v8::Value> IDLDictionaryBase::toV8Impl(v8::Local<v8::Object>,
                                                 v8::Isolate*) const {
  NOTREACHED();
  return v8::Local<v8::Value>();
}

DEFINE_TRACE(IDLDictionaryBase) {}

}  // namespace blink
