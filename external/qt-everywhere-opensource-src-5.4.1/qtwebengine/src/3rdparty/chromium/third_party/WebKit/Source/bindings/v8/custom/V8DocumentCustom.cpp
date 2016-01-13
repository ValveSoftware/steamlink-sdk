/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
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
#include "bindings/core/v8/V8Document.h"

#include "bindings/core/v8/V8CanvasRenderingContext2D.h"
#include "bindings/core/v8/V8DOMImplementation.h"
#include "bindings/core/v8/V8Node.h"
#include "bindings/core/v8/V8Touch.h"
#include "bindings/core/v8/V8TouchList.h"
#include "bindings/core/v8/V8WebGLRenderingContext.h"
#include "bindings/core/v8/V8XPathNSResolver.h"
#include "bindings/core/v8/V8XPathResult.h"
#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8DOMWrapper.h"
#include "bindings/v8/custom/V8CustomXPathNSResolver.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/Node.h"
#include "core/dom/TouchList.h"
#include "core/frame/LocalFrame.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/xml/DocumentXPathEvaluator.h"
#include "core/xml/XPathNSResolver.h"
#include "core/xml/XPathResult.h"
#include "wtf/RefPtr.h"

namespace WebCore {

void V8Document::evaluateMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    RefPtrWillBeRawPtr<Document> document = V8Document::toNative(info.Holder());
    ASSERT(document);
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "evaluate", "Document", info.Holder(), info.GetIsolate());
    TOSTRING_VOID(V8StringResource<>, expression, info[0]);
    RefPtrWillBeRawPtr<Node> contextNode = V8Node::toNativeWithTypeCheck(info.GetIsolate(), info[1]);

    const int resolverArgumentIndex = 2;
    RefPtrWillBeRawPtr<XPathNSResolver> resolver = toXPathNSResolver(info[resolverArgumentIndex], info.GetIsolate());
    if (!resolver && !isUndefinedOrNull(info[resolverArgumentIndex])) {
        exceptionState.throwTypeError(ExceptionMessages::argumentNullOrIncorrectType(resolverArgumentIndex + 1, "XPathNSResolver"));
        exceptionState.throwIfNeeded();
        return;
    }

    int type = toInt32(info[3]);
    RefPtrWillBeRawPtr<XPathResult> inResult = V8XPathResult::toNativeWithTypeCheck(info.GetIsolate(), info[4]);
    TONATIVE_VOID(RefPtrWillBeRawPtr<XPathResult>, result, DocumentXPathEvaluator::evaluate(*document, expression, contextNode.get(), resolver.release(), type, inResult.get(), exceptionState));
    if (exceptionState.throwIfNeeded())
        return;

    v8SetReturnValueFast(info, result.release(), document.get());
}

} // namespace WebCore
