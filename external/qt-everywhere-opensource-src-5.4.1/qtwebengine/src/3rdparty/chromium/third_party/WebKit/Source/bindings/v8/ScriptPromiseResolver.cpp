/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "bindings/v8/ScriptPromiseResolver.h"

#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8DOMWrapper.h"

#include <v8.h>

namespace WebCore {

ScriptPromiseResolver::ScriptPromiseResolver(ScriptState* scriptState)
    : m_scriptState(scriptState)
{
    v8::Isolate* isolate = m_scriptState->isolate();
    ASSERT(!m_scriptState->contextIsEmpty());
    ASSERT(isolate->InContext());
    m_resolver = ScriptValue(scriptState, v8::Promise::Resolver::New(isolate));
}

ScriptPromiseResolver::~ScriptPromiseResolver()
{
    // We don't call "reject" here because it requires a caller
    // to be in a v8 context.

    m_resolver.clear();
}

ScriptPromise ScriptPromiseResolver::promise()
{
    ASSERT(!m_scriptState->contextIsEmpty());
    ASSERT(m_scriptState->isolate()->InContext());
    if (!m_resolver.isEmpty()) {
        v8::Local<v8::Promise::Resolver> v8Resolver = m_resolver.v8Value().As<v8::Promise::Resolver>();
        return ScriptPromise(m_scriptState.get(), v8Resolver->GetPromise());
    }
    return ScriptPromise();
}

PassRefPtr<ScriptPromiseResolver> ScriptPromiseResolver::create(ScriptState* scriptState)
{
    ASSERT(scriptState->isolate()->InContext());
    return adoptRef(new ScriptPromiseResolver(scriptState));
}

void ScriptPromiseResolver::resolve(v8::Handle<v8::Value> value)
{
    ASSERT(m_scriptState->isolate()->InContext());
    if (!m_resolver.isEmpty()) {
        m_resolver.v8Value().As<v8::Promise::Resolver>()->Resolve(value);
    }
    m_resolver.clear();
}

void ScriptPromiseResolver::reject(v8::Handle<v8::Value> value)
{
    ASSERT(m_scriptState->isolate()->InContext());
    if (!m_resolver.isEmpty()) {
        m_resolver.v8Value().As<v8::Promise::Resolver>()->Reject(value);
    }
    m_resolver.clear();
}

} // namespace WebCore
