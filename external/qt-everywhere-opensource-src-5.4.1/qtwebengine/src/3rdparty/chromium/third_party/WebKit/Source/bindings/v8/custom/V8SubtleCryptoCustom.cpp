// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/modules/v8/V8SubtleCrypto.h"

#include "bindings/modules/v8/V8Key.h"
#include "bindings/v8/Dictionary.h"
#include "bindings/v8/custom/V8ArrayBufferCustom.h"
#include "bindings/v8/custom/V8ArrayBufferViewCustom.h"

namespace WebCore {

////////////////////////////////////////////////////////////////////////////////
// Overload resolution for verify()
// FIXME: needs support for union types http://crbug.com/240176
////////////////////////////////////////////////////////////////////////////////

// Promise verify(Dictionary algorithm, Key key, ArrayBuffer signature, ArrayBuffer data);
void verify1Method(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    SubtleCrypto* impl = V8SubtleCrypto::toNative(info.Holder());
    TONATIVE_VOID(Dictionary, algorithm, Dictionary(info[0], info.GetIsolate()));
    if (!algorithm.isUndefinedOrNull() && !algorithm.isObject()) {
        throwTypeError(ExceptionMessages::failedToExecute("verify", "SubtleCrypto", "parameter 1 ('algorithm') is not an object."), info.GetIsolate());
        return;
    }
    TONATIVE_VOID(Key*, key, V8Key::toNativeWithTypeCheck(info.GetIsolate(), info[1]));
    TONATIVE_VOID(ArrayBuffer*, signature, info[2]->IsArrayBuffer() ? V8ArrayBuffer::toNative(v8::Handle<v8::ArrayBuffer>::Cast(info[2])) : 0);
    TONATIVE_VOID(ArrayBuffer*, data, info[3]->IsArrayBuffer() ? V8ArrayBuffer::toNative(v8::Handle<v8::ArrayBuffer>::Cast(info[3])) : 0);
    v8SetReturnValue(info, impl->verifySignature(ScriptState::current(info.GetIsolate()), algorithm, key, signature, data).v8Value());
}

// Promise verify(Dictionary algorithm, Key key, ArrayBuffer signature, ArrayBufferView data);
void verify2Method(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    SubtleCrypto* impl = V8SubtleCrypto::toNative(info.Holder());
    TONATIVE_VOID(Dictionary, algorithm, Dictionary(info[0], info.GetIsolate()));
    if (!algorithm.isUndefinedOrNull() && !algorithm.isObject()) {
        throwTypeError(ExceptionMessages::failedToExecute("verify", "SubtleCrypto", "parameter 1 ('algorithm') is not an object."), info.GetIsolate());
        return;
    }
    TONATIVE_VOID(Key*, key, V8Key::toNativeWithTypeCheck(info.GetIsolate(), info[1]));
    TONATIVE_VOID(ArrayBuffer*, signature, info[2]->IsArrayBuffer() ? V8ArrayBuffer::toNative(v8::Handle<v8::ArrayBuffer>::Cast(info[2])) : 0);
    TONATIVE_VOID(ArrayBufferView*, data, info[3]->IsArrayBufferView() ? V8ArrayBufferView::toNative(v8::Handle<v8::ArrayBufferView>::Cast(info[3])) : 0);
    v8SetReturnValue(info, impl->verifySignature(ScriptState::current(info.GetIsolate()), algorithm, key, signature, data).v8Value());
}

// Promise verify(Dictionary algorithm, Key key, ArrayBufferView signature, ArrayBuffer data);
void verify3Method(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    SubtleCrypto* impl = V8SubtleCrypto::toNative(info.Holder());
    TONATIVE_VOID(Dictionary, algorithm, Dictionary(info[0], info.GetIsolate()));
    if (!algorithm.isUndefinedOrNull() && !algorithm.isObject()) {
        throwTypeError(ExceptionMessages::failedToExecute("verify", "SubtleCrypto", "parameter 1 ('algorithm') is not an object."), info.GetIsolate());
        return;
    }
    TONATIVE_VOID(Key*, key, V8Key::toNativeWithTypeCheck(info.GetIsolate(), info[1]));
    TONATIVE_VOID(ArrayBufferView*, signature, info[2]->IsArrayBufferView() ? V8ArrayBufferView::toNative(v8::Handle<v8::ArrayBufferView>::Cast(info[2])) : 0);
    TONATIVE_VOID(ArrayBuffer*, data, info[3]->IsArrayBuffer() ? V8ArrayBuffer::toNative(v8::Handle<v8::ArrayBuffer>::Cast(info[3])) : 0);
    v8SetReturnValue(info, impl->verifySignature(ScriptState::current(info.GetIsolate()), algorithm, key, signature, data).v8Value());
}

// Promise verify(Dictionary algorithm, Key key, ArrayBufferView signature, ArrayBufferView data);
void verify4Method(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    SubtleCrypto* impl = V8SubtleCrypto::toNative(info.Holder());
    TONATIVE_VOID(Dictionary, algorithm, Dictionary(info[0], info.GetIsolate()));
    if (!algorithm.isUndefinedOrNull() && !algorithm.isObject()) {
        throwTypeError(ExceptionMessages::failedToExecute("verify", "SubtleCrypto", "parameter 1 ('algorithm') is not an object."), info.GetIsolate());
        return;
    }
    TONATIVE_VOID(Key*, key, V8Key::toNativeWithTypeCheck(info.GetIsolate(), info[1]));
    TONATIVE_VOID(ArrayBufferView*, signature, info[2]->IsArrayBufferView() ? V8ArrayBufferView::toNative(v8::Handle<v8::ArrayBufferView>::Cast(info[2])) : 0);
    TONATIVE_VOID(ArrayBufferView*, data, info[3]->IsArrayBufferView() ? V8ArrayBufferView::toNative(v8::Handle<v8::ArrayBufferView>::Cast(info[3])) : 0);
    v8SetReturnValue(info, impl->verifySignature(ScriptState::current(info.GetIsolate()), algorithm, key, signature, data).v8Value());
}

void V8SubtleCrypto::verifyMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* isolate = info.GetIsolate();
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "verify", "SubtleCrypto", info.Holder(), isolate);
    // typedef (ArrayBuffer or ArrayBufferView) CryptoOperationData;
    //
    // Promise verify(Dictionary algorithm, Key key,
    //                CryptoOperationData signature,
    //                CryptoOperationData data);
    switch (info.Length()) {
    case 4:
        // Promise verify(Dictionary algorithm, Key key, ArrayBuffer signature, ArrayBuffer data);
        if (V8ArrayBuffer::hasInstance(info[2], isolate)
            && V8ArrayBuffer::hasInstance(info[3], isolate)) {
            verify1Method(info);
            return;
        }
        // Promise verify(Dictionary algorithm, Key key, ArrayBuffer signature, ArrayBufferView data);
        if (V8ArrayBuffer::hasInstance(info[2], isolate)
            && V8ArrayBufferView::hasInstance(info[3], isolate)) {
            verify2Method(info);
            return;
        }
        // Promise verify(Dictionary algorithm, Key key, ArrayBufferView signature, ArrayBuffer data);
        if (V8ArrayBufferView::hasInstance(info[2], isolate)
            && V8ArrayBuffer::hasInstance(info[3], isolate)) {
            verify3Method(info);
            return;
        }
        // Promise verify(Dictionary algorithm, Key key, ArrayBufferView signature, ArrayBufferView data);
        if (V8ArrayBufferView::hasInstance(info[2], isolate)
            && V8ArrayBufferView::hasInstance(info[3], isolate)) {
            verify4Method(info);
            return;
        }
        break;
    default:
        throwArityTypeError(exceptionState, "[4]", info.Length());
        return;
        break;
    }
    exceptionState.throwTypeError("No function was found that matched the signature provided.");
    exceptionState.throwIfNeeded();
}

} // namespace WebCore
