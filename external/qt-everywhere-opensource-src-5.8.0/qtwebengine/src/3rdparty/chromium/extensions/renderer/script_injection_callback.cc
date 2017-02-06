// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/script_injection_callback.h"

namespace extensions {

ScriptInjectionCallback::ScriptInjectionCallback(
    const CompleteCallback& injection_completed_callback)
    : injection_completed_callback_(injection_completed_callback) {
}

ScriptInjectionCallback::~ScriptInjectionCallback() {
}

void ScriptInjectionCallback::completed(
    const blink::WebVector<v8::Local<v8::Value> >& result) {
  injection_completed_callback_.Run(result);
  delete this;
}

}  // namespace extensions
