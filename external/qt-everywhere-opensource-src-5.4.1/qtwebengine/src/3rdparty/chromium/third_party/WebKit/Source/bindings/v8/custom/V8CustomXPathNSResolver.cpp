// Copyright (c) 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "bindings/v8/custom/V8CustomXPathNSResolver.h"

#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8Binding.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/ScriptCallStack.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<V8CustomXPathNSResolver> V8CustomXPathNSResolver::create(v8::Handle<v8::Object> resolver, v8::Isolate* isolate)
{
    return adoptRefWillBeNoop(new V8CustomXPathNSResolver(resolver, isolate));
}

V8CustomXPathNSResolver::V8CustomXPathNSResolver(v8::Handle<v8::Object> resolver, v8::Isolate* isolate)
    : m_resolver(resolver)
    , m_isolate(isolate)
{
}

V8CustomXPathNSResolver::~V8CustomXPathNSResolver()
{
}

AtomicString V8CustomXPathNSResolver::lookupNamespaceURI(const String& prefix)
{
    v8::Handle<v8::Function> lookupNamespaceURIFunc;
    v8::Handle<v8::String> lookupNamespaceURIName = v8AtomicString(m_isolate, "lookupNamespaceURI");

    // Check if the resolver has a function property named lookupNamespaceURI.
    if (m_resolver->Has(lookupNamespaceURIName)) {
        v8::Handle<v8::Value> lookupNamespaceURI = m_resolver->Get(lookupNamespaceURIName);
        if (lookupNamespaceURI->IsFunction())
            lookupNamespaceURIFunc = v8::Handle<v8::Function>::Cast(lookupNamespaceURI);
    }

    if (lookupNamespaceURIFunc.IsEmpty() && !m_resolver->IsFunction()) {
        LocalFrame* frame = callingDOMWindow(m_isolate)->frame();
        if (frame && frame->host())
            frame->console().addMessage(JSMessageSource, ErrorMessageLevel, "XPathNSResolver does not have a lookupNamespaceURI method.");
        return nullAtom;
    }

    // Catch exceptions from calling the namespace resolver.
    v8::TryCatch tryCatch;
    tryCatch.SetVerbose(true); // Print exceptions to console.

    const int argc = 1;
    v8::Handle<v8::Value> argv[argc] = { v8String(m_isolate, prefix) };
    v8::Handle<v8::Function> function = lookupNamespaceURIFunc.IsEmpty() ? v8::Handle<v8::Function>::Cast(m_resolver) : lookupNamespaceURIFunc;

    v8::Handle<v8::Value> retval = ScriptController::callFunction(callingExecutionContext(m_isolate), function, m_resolver, argc, argv, m_isolate);

    // Eat exceptions from namespace resolver and return an empty string. This will most likely cause NamespaceError.
    if (tryCatch.HasCaught())
        return nullAtom;

    TOSTRING_DEFAULT(V8StringResource<WithNullCheck>, returnString, retval, nullAtom);
    return returnString;
}

void V8CustomXPathNSResolver::trace(Visitor* visitor)
{
    XPathNSResolver::trace(visitor);
}

} // namespace WebCore
