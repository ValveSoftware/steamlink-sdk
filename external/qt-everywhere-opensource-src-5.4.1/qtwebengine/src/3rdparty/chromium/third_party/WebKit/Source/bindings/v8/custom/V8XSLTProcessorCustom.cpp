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

#include "config.h"
#include "bindings/core/v8/V8XSLTProcessor.h"

#include "bindings/core/v8/V8Document.h"
#include "bindings/core/v8/V8DocumentFragment.h"
#include "bindings/core/v8/V8Node.h"
#include "bindings/v8/V8Binding.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/Node.h"
#include "core/xml/XSLTProcessor.h"
#include "wtf/RefPtr.h"

namespace WebCore {

void V8XSLTProcessor::setParameterMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (isUndefinedOrNull(info[1]) || isUndefinedOrNull(info[2]))
        return;

    TOSTRING_VOID(V8StringResource<>, namespaceURI, info[0]);
    TOSTRING_VOID(V8StringResource<>, localName, info[1]);
    TOSTRING_VOID(V8StringResource<>, value, info[2]);

    XSLTProcessor* impl = V8XSLTProcessor::toNative(info.Holder());
    impl->setParameter(namespaceURI, localName, value);
}

void V8XSLTProcessor::getParameterMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (isUndefinedOrNull(info[1]))
        return;

    TOSTRING_VOID(V8StringResource<>, namespaceURI, info[0]);
    TOSTRING_VOID(V8StringResource<>, localName, info[1]);

    XSLTProcessor* impl = V8XSLTProcessor::toNative(info.Holder());
    String result = impl->getParameter(namespaceURI, localName);
    if (result.isNull())
        return;

    v8SetReturnValueString(info, result, info.GetIsolate());
}

void V8XSLTProcessor::removeParameterMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (isUndefinedOrNull(info[1]))
        return;

    TOSTRING_VOID(V8StringResource<>, namespaceURI, info[0]);
    TOSTRING_VOID(V8StringResource<>, localName, info[1]);

    XSLTProcessor* impl = V8XSLTProcessor::toNative(info.Holder());
    impl->removeParameter(namespaceURI, localName);
}

} // namespace WebCore
