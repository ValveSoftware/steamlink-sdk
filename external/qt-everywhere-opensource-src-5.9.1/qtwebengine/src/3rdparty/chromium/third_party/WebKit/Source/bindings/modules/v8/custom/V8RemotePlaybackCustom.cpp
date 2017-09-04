// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/V8RemotePlayback.h"

#include "bindings/core/v8/V8DOMWrapper.h"
#include "modules/remoteplayback/RemotePlayback.h"

namespace blink {

void V8RemotePlayback::visitDOMWrapperCustom(
    v8::Isolate* isolate,
    ScriptWrappable* scriptWrappable,
    const v8::Persistent<v8::Object>& wrapper) {
  RemotePlayback* remotePlayback = scriptWrappable->toImpl<RemotePlayback>();
  remotePlayback->setV8ReferencesForCallbacks(isolate, wrapper);
}

}  // namespace blink
