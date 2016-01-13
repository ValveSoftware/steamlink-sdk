// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/modules/v8/V8ServiceWorker.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "core/dom/MessagePort.h"
#include "modules/serviceworkers/ServiceWorker.h"
#include "wtf/ArrayBuffer.h"

namespace WebCore {

void V8ServiceWorker::postMessageMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "postMessage", "ServiceWorker", info.Holder(), info.GetIsolate());
    ServiceWorker* worker = V8ServiceWorker::toNative(info.Holder());
    MessagePortArray ports;
    ArrayBufferArray arrayBuffers;
    if (info.Length() > 1) {
        const int transferablesArgIndex = 1;
        if (!SerializedScriptValue::extractTransferables(info[transferablesArgIndex], transferablesArgIndex, ports, arrayBuffers, exceptionState, info.GetIsolate())) {
            exceptionState.throwIfNeeded();
            return;
        }
    }
    RefPtr<SerializedScriptValue> message = SerializedScriptValue::create(info[0], &ports, &arrayBuffers, exceptionState, info.GetIsolate());
    if (exceptionState.throwIfNeeded())
        return;
    worker->postMessage(message.release(), &ports, exceptionState);
    exceptionState.throwIfNeeded();
}

} // namespace WebCore
