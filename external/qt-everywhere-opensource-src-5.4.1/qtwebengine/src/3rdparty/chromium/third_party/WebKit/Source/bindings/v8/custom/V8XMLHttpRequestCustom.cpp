/*
 * Copyright (C) 2008, 2009, 2010 Google Inc. All rights reserved.
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

#include "config.h"
#include "bindings/core/v8/V8XMLHttpRequest.h"

#include "bindings/core/v8/V8Blob.h"
#include "bindings/core/v8/V8Document.h"
#include "bindings/core/v8/V8FormData.h"
#include "bindings/core/v8/V8HTMLDocument.h"
#include "bindings/core/v8/V8Stream.h"
#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/custom/V8ArrayBufferCustom.h"
#include "bindings/v8/custom/V8ArrayBufferViewCustom.h"
#include "core/dom/Document.h"
#include "core/fileapi/Stream.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/xml/XMLHttpRequest.h"
#include "wtf/ArrayBuffer.h"
#include <v8.h>

namespace WebCore {

void V8XMLHttpRequest::constructorCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExecutionContext* context = currentExecutionContext(info.GetIsolate());

    RefPtr<SecurityOrigin> securityOrigin;
    if (context->isDocument()) {
        DOMWrapperWorld& world = DOMWrapperWorld::current(info.GetIsolate());
        if (world.isIsolatedWorld())
            securityOrigin = world.isolatedWorldSecurityOrigin();
    }

    RefPtrWillBeRawPtr<XMLHttpRequest> xmlHttpRequest = XMLHttpRequest::create(context, securityOrigin);

    v8::Handle<v8::Object> wrapper = info.Holder();
    V8DOMWrapper::associateObjectWithWrapper<V8XMLHttpRequest>(xmlHttpRequest.release(), &wrapperTypeInfo, wrapper, info.GetIsolate(), WrapperConfiguration::Dependent);
    info.GetReturnValue().Set(wrapper);
}

void V8XMLHttpRequest::responseTextAttributeGetterCustom(const v8::PropertyCallbackInfo<v8::Value>& info)
{
    XMLHttpRequest* xmlHttpRequest = V8XMLHttpRequest::toNative(info.Holder());
    ExceptionState exceptionState(ExceptionState::GetterContext, "responseText", "XMLHttpRequest", info.Holder(), info.GetIsolate());
    ScriptString text = xmlHttpRequest->responseText(exceptionState);
    if (exceptionState.throwIfNeeded())
        return;
    if (text.isEmpty()) {
        v8SetReturnValueString(info, emptyString(), info.GetIsolate());
        return;
    }
    v8SetReturnValue(info, text.v8Value());
}

void V8XMLHttpRequest::responseAttributeGetterCustom(const v8::PropertyCallbackInfo<v8::Value>& info)
{
    XMLHttpRequest* xmlHttpRequest = V8XMLHttpRequest::toNative(info.Holder());

    switch (xmlHttpRequest->responseTypeCode()) {
    case XMLHttpRequest::ResponseTypeDefault:
    case XMLHttpRequest::ResponseTypeText:
        responseTextAttributeGetterCustom(info);
        return;

    case XMLHttpRequest::ResponseTypeJSON:
        {
            v8::Isolate* isolate = info.GetIsolate();

            ScriptString jsonSource = xmlHttpRequest->responseJSONSource();
            if (jsonSource.isEmpty()) {
                v8SetReturnValue(info, v8::Null(isolate));
                return;
            }

            // Catch syntax error.
            v8::TryCatch exceptionCatcher;
            v8::Handle<v8::Value> json = v8::JSON::Parse(jsonSource.v8Value());
            if (exceptionCatcher.HasCaught() || json.IsEmpty())
                v8SetReturnValue(info, v8::Null(isolate));
            else
                v8SetReturnValue(info, json);
            return;
        }

    case XMLHttpRequest::ResponseTypeDocument:
        {
            ExceptionState exceptionState(ExceptionState::GetterContext, "response", "XMLHttpRequest", info.Holder(), info.GetIsolate());
            Document* document = xmlHttpRequest->responseXML(exceptionState);
            if (exceptionState.throwIfNeeded())
                return;
            v8SetReturnValueFast(info, document, xmlHttpRequest);
            return;
        }

    case XMLHttpRequest::ResponseTypeBlob:
        {
            Blob* blob = xmlHttpRequest->responseBlob();
            v8SetReturnValueFast(info, blob, xmlHttpRequest);
            return;
        }

    case XMLHttpRequest::ResponseTypeStream:
        {
            Stream* stream = xmlHttpRequest->responseStream();
            v8SetReturnValueFast(info, stream, xmlHttpRequest);
            return;
        }

    case XMLHttpRequest::ResponseTypeArrayBuffer:
        {
            ArrayBuffer* arrayBuffer = xmlHttpRequest->responseArrayBuffer();
            if (arrayBuffer) {
                arrayBuffer->setDeallocationObserver(V8ArrayBufferDeallocationObserver::instanceTemplate());
            }
            v8SetReturnValueFast(info, arrayBuffer, xmlHttpRequest);
            return;
        }
    }
}

void V8XMLHttpRequest::openMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    // Four cases:
    // open(method, url)
    // open(method, url, async)
    // open(method, url, async, user)
    // open(method, url, async, user, passwd)

    ExceptionState exceptionState(ExceptionState::ExecutionContext, "open", "XMLHttpRequest", info.Holder(), info.GetIsolate());

    if (info.Length() < 2) {
        exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments(2, info.Length()));
        exceptionState.throwIfNeeded();
        return;
    }

    XMLHttpRequest* xmlHttpRequest = V8XMLHttpRequest::toNative(info.Holder());

    TOSTRING_VOID(V8StringResource<>, method, info[0]);
    TOSTRING_VOID(V8StringResource<>, urlstring, info[1]);

    ExecutionContext* context = currentExecutionContext(info.GetIsolate());
    KURL url = context->completeURL(urlstring);

    if (info.Length() >= 3) {
        bool async = info[2]->BooleanValue();

        if (info.Length() >= 4 && !info[3]->IsUndefined()) {
            TOSTRING_VOID(V8StringResource<WithNullCheck>, user, info[3]);

            if (info.Length() >= 5 && !info[4]->IsUndefined()) {
                TOSTRING_VOID(V8StringResource<WithNullCheck>, password, info[4]);
                xmlHttpRequest->open(method, url, async, user, password, exceptionState);
            } else {
                xmlHttpRequest->open(method, url, async, user, exceptionState);
            }
        } else {
            xmlHttpRequest->open(method, url, async, exceptionState);
        }
    } else {
        xmlHttpRequest->open(method, url, exceptionState);
    }

    exceptionState.throwIfNeeded();
}

static bool isDocumentType(v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    // FIXME: add other document types.
    return V8Document::hasInstance(value, isolate) || V8HTMLDocument::hasInstance(value, isolate);
}

void V8XMLHttpRequest::sendMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    XMLHttpRequest* xmlHttpRequest = V8XMLHttpRequest::toNative(info.Holder());

    InspectorInstrumentation::willSendXMLHttpRequest(xmlHttpRequest->executionContext(), xmlHttpRequest->url());

    ExceptionState exceptionState(ExceptionState::ExecutionContext, "send", "XMLHttpRequest", info.Holder(), info.GetIsolate());
    if (info.Length() < 1)
        xmlHttpRequest->send(exceptionState);
    else {
        v8::Handle<v8::Value> arg = info[0];
        if (isUndefinedOrNull(arg)) {
            xmlHttpRequest->send(exceptionState);
        } else if (isDocumentType(arg, info.GetIsolate())) {
            v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(arg);
            Document* document = V8Document::toNative(object);
            ASSERT(document);
            xmlHttpRequest->send(document, exceptionState);
        } else if (V8Blob::hasInstance(arg, info.GetIsolate())) {
            v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(arg);
            Blob* blob = V8Blob::toNative(object);
            ASSERT(blob);
            xmlHttpRequest->send(blob, exceptionState);
        } else if (V8FormData::hasInstance(arg, info.GetIsolate())) {
            v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(arg);
            DOMFormData* domFormData = V8FormData::toNative(object);
            ASSERT(domFormData);
            xmlHttpRequest->send(domFormData, exceptionState);
        } else if (V8ArrayBuffer::hasInstance(arg, info.GetIsolate())) {
            v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(arg);
            ArrayBuffer* arrayBuffer = V8ArrayBuffer::toNative(object);
            ASSERT(arrayBuffer);
            xmlHttpRequest->send(arrayBuffer, exceptionState);
        } else if (V8ArrayBufferView::hasInstance(arg, info.GetIsolate())) {
            v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(arg);
            ArrayBufferView* arrayBufferView = V8ArrayBufferView::toNative(object);
            ASSERT(arrayBufferView);
            xmlHttpRequest->send(arrayBufferView, exceptionState);
        } else {
            TOSTRING_VOID(V8StringResource<WithNullCheck>, argString, arg);
            xmlHttpRequest->send(argString, exceptionState);
        }
    }

    exceptionState.throwIfNeeded();
}

} // namespace WebCore
