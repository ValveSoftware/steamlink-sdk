// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8HiddenValue_h
#define V8HiddenValue_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptPromiseProperties.h"
#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/PtrUtil.h"
#include <memory>
#include <v8.h>

namespace blink {

class ScriptState;
class ScriptWrappable;

#define V8_HIDDEN_VALUES(V) \
    V(customElementAttachedCallback) \
    V(customElementAttributeChangedCallback) \
    V(customElementCreatedCallback) \
    V(customElementDetachedCallback) \
    V(customElementDocument) \
    V(customElementIsInterfacePrototypeObject) \
    V(customElementNamespaceURI) \
    V(customElementTagName) \
    V(customElementType) \
    V(customElementsRegistryMap) \
    V(event) \
    V(idbCursorRequest) \
    V(internalBodyBuffer) \
    V(internalBodyStream) \
    V(port1) \
    V(port2) \
    V(readableStreamReaderInResponse) \
    V(requestInFetchEvent) \
    V(state) \
    V(testInterfaces) \
    V(thenableHiddenPromise) \
    V(toStringString) \
    V(injectedScriptNative) \
    V(webgl2DTextures) \
    V(webgl2DArrayTextures) \
    V(webgl3DTextures) \
    V(webglAttachments) \
    V(webglBuffers) \
    V(webglCubeMapTextures) \
    V(webglExtensions) \
    V(webglMisc) \
    V(webglQueries) \
    V(webglSamplers) \
    V(webglShaders) \
    SCRIPT_PROMISE_PROPERTIES(V, Promise)  \
    SCRIPT_PROMISE_PROPERTIES(V, Resolver)

class CORE_EXPORT V8HiddenValue {
    USING_FAST_MALLOC(V8HiddenValue);
    WTF_MAKE_NONCOPYABLE(V8HiddenValue);
public:
    static std::unique_ptr<V8HiddenValue> create() { return wrapUnique(new V8HiddenValue()); }

#define V8_DECLARE_METHOD(name) static v8::Local<v8::String> name(v8::Isolate* isolate);
    V8_HIDDEN_VALUES(V8_DECLARE_METHOD);
#undef V8_DECLARE_METHOD

    static v8::Local<v8::Value> getHiddenValue(ScriptState*, v8::Local<v8::Object>, v8::Local<v8::String>);
    static bool setHiddenValue(ScriptState*, v8::Local<v8::Object>, v8::Local<v8::String>, v8::Local<v8::Value>);
    static bool deleteHiddenValue(ScriptState*, v8::Local<v8::Object>, v8::Local<v8::String>);
    static v8::Local<v8::Value> getHiddenValueFromMainWorldWrapper(ScriptState*, ScriptWrappable*, v8::Local<v8::String>);

private:
    V8HiddenValue() { }

#define V8_DECLARE_FIELD(name) ScopedPersistent<v8::String> m_##name;
    V8_HIDDEN_VALUES(V8_DECLARE_FIELD);
#undef V8_DECLARE_FIELD
};

} // namespace blink

#endif // V8HiddenValue_h
