// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/V8ExtendableMessageEvent.h"

#include "bindings/modules/v8/V8ExtendableMessageEventInit.h"
#include "bindings/modules/v8/V8ServiceWorkerMessageEventInternal.h"

namespace blink {

void V8ExtendableMessageEvent::constructorCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    V8ServiceWorkerMessageEventInternal::constructorCustom<ExtendableMessageEvent, ExtendableMessageEventInit>(info);
}

void V8ExtendableMessageEvent::dataAttributeGetterCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    V8ServiceWorkerMessageEventInternal::dataAttributeGetterCustom<ExtendableMessageEvent>(info);
}

} // namespace blink
