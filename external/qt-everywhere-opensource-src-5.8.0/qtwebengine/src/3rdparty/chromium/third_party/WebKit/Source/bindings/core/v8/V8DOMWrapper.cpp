/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings/core/v8/V8DOMWrapper.h"

#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8Location.h"
#include "bindings/core/v8/V8ObjectConstructor.h"
#include "bindings/core/v8/V8PerContextData.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/V8Window.h"
#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"

namespace blink {

v8::Local<v8::Object> V8DOMWrapper::createWrapper(v8::Isolate* isolate, v8::Local<v8::Object> creationContext, const WrapperTypeInfo* type)
{
    ASSERT(!type->equals(&V8Window::wrapperTypeInfo));
    // According to https://html.spec.whatwg.org/multipage/browsers.html#security-location,
    // cross-origin script access to a few properties of Location is allowed.
    // Location already implements the necessary security checks.
    bool withSecurityCheck = !type->equals(&V8Location::wrapperTypeInfo);
    V8WrapperInstantiationScope scope(creationContext, isolate, withSecurityCheck);

    V8PerContextData* perContextData = V8PerContextData::from(scope.context());
    v8::Local<v8::Object> wrapper;
    if (perContextData) {
        wrapper = perContextData->createWrapperFromCache(type);
    } else {
        // The context is detached, but still accessible.
        // TODO(yukishiino): This code does not create a wrapper with
        // the correct settings.  Should follow the same way as
        // V8PerContextData::createWrapperFromCache, though there is no need to
        // cache resulting objects or their constructors.
        const DOMWrapperWorld& world = DOMWrapperWorld::world(scope.context());
        if (!type->domTemplate(isolate, world)->InstanceTemplate()->NewInstance(scope.context()).ToLocal(&wrapper)) {
            // Nothing to do.
        }
    }

    return wrapper;
}

bool V8DOMWrapper::isWrapper(v8::Isolate* isolate, v8::Local<v8::Value> value)
{
    if (value.IsEmpty() || !value->IsObject())
        return false;
    v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);

    if (object->InternalFieldCount() < v8DefaultWrapperInternalFieldCount)
        return false;

    const WrapperTypeInfo* untrustedWrapperTypeInfo = toWrapperTypeInfo(object);
    V8PerIsolateData* perIsolateData = V8PerIsolateData::from(isolate);
    if (!(untrustedWrapperTypeInfo && perIsolateData))
        return false;
    return perIsolateData->hasInstance(untrustedWrapperTypeInfo, object);
}

bool V8DOMWrapper::hasInternalFieldsSet(v8::Local<v8::Value> value)
{
    if (value.IsEmpty() || !value->IsObject())
        return false;
    v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);

    if (object->InternalFieldCount() < v8DefaultWrapperInternalFieldCount)
        return false;

    const ScriptWrappable* untrustedScriptWrappable = toScriptWrappable(object);
    const WrapperTypeInfo* untrustedWrapperTypeInfo = toWrapperTypeInfo(object);
    return untrustedScriptWrappable
        && untrustedWrapperTypeInfo
        && untrustedWrapperTypeInfo->ginEmbedder == gin::kEmbedderBlink;
}

void V8WrapperInstantiationScope::securityCheck(v8::Isolate* isolate, v8::Local<v8::Context> contextForWrapper)
{
    if (m_context.IsEmpty())
        return;
    // If the context is different, we need to make sure that the current
    // context has access to the creation context.
    Frame* frame = toFrameIfNotDetached(contextForWrapper);
    if (!frame) {
        // Sandbox detached frames - they can't create cross origin objects.
        LocalDOMWindow* callingWindow = currentDOMWindow(isolate);
        DOMWindow* targetWindow = toDOMWindow(contextForWrapper);
        if (callingWindow->document()->getSecurityOrigin()->canAccessCheckSuborigins(targetWindow->document()->getSecurityOrigin()))
            return;

        // TODO(jochen): Currently, Location is the only object for which we can reach this code path. Should be generalized.
        ExceptionState exceptionState(ExceptionState::ConstructionContext, "Location", contextForWrapper->Global(), isolate);
        // We can't create a better message for a detached frame.
        exceptionState.throwSecurityError(String(), String());
        exceptionState.throwIfNeeded();
        return;
    }
    const DOMWrapperWorld& currentWorld = DOMWrapperWorld::world(m_context);
    RELEASE_ASSERT(currentWorld.worldId() == DOMWrapperWorld::world(contextForWrapper).worldId());
    if (currentWorld.isMainWorld()) {
        RELEASE_ASSERT(BindingSecurity::shouldAllowAccessToFrame(isolate, currentDOMWindow(isolate), frame, DoNotReportSecurityError));
    }
}

void V8WrapperInstantiationScope::convertException()
{
    v8::Isolate* isolate = m_context->GetIsolate();
    // TODO(jochen): Currently, Location is the only object for which we can reach this code path. Should be generalized.
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "Location", isolate->GetCurrentContext()->Global(), isolate);
    LocalDOMWindow* callingWindow = currentDOMWindow(isolate);
    DOMWindow* targetWindow = toDOMWindow(m_context);
    exceptionState.throwSecurityError(targetWindow->sanitizedCrossDomainAccessErrorMessage(callingWindow), targetWindow->crossDomainAccessErrorMessage(callingWindow));
    exceptionState.throwIfNeeded();
}

} // namespace blink
