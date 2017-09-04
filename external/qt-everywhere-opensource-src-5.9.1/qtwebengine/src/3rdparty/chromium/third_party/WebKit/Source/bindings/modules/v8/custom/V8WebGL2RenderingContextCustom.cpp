// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/V8WebGL2RenderingContext.h"

#include "modules/webgl/WebGL2RenderingContextBase.h"

using namespace WTF;

namespace blink {

void V8WebGL2RenderingContext::visitDOMWrapperCustom(
    v8::Isolate* isolate,
    ScriptWrappable* scriptWrappable,
    const v8::Persistent<v8::Object>& wrapper) {
  WebGL2RenderingContext* impl =
      scriptWrappable->toImpl<WebGL2RenderingContext>();
  impl->visitChildDOMWrappers(isolate, wrapper);
}

}  // namespace blink
