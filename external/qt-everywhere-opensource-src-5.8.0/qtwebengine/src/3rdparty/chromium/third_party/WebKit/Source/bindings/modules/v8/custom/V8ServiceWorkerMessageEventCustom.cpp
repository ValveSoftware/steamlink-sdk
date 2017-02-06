// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/V8ServiceWorkerMessageEvent.h"

#include "bindings/modules/v8/V8ServiceWorkerMessageEventInit.h"
#include "bindings/modules/v8/V8ServiceWorkerMessageEventInternal.h"

namespace blink {

void V8ServiceWorkerMessageEvent::constructorCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    V8ServiceWorkerMessageEventInternal::constructorCustom<ServiceWorkerMessageEvent, ServiceWorkerMessageEventInit>(info);
}

void V8ServiceWorkerMessageEvent::dataAttributeGetterCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    V8ServiceWorkerMessageEventInternal::dataAttributeGetterCustom<ServiceWorkerMessageEvent>(info);
}

} // namespace blink
