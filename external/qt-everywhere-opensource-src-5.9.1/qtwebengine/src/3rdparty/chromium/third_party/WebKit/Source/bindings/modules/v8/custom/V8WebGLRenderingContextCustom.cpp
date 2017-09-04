// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/V8WebGLRenderingContext.h"

#include "modules/webgl/WebGLRenderingContextBase.h"

using namespace WTF;

namespace blink {

void V8WebGLRenderingContext::visitDOMWrapperCustom(
    v8::Isolate* isolate,
    ScriptWrappable* scriptWrappable,
    const v8::Persistent<v8::Object>& wrapper) {
  WebGLRenderingContext* impl =
      scriptWrappable->toImpl<WebGLRenderingContext>();
  impl->visitChildDOMWrappers(isolate, wrapper);
}

}  // namespace blink
