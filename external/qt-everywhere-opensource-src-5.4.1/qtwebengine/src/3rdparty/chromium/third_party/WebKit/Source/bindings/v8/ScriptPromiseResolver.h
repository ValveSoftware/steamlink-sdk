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

#ifndef ScriptPromiseResolver_h
#define ScriptPromiseResolver_h

#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptState.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "wtf/RefPtr.h"

#include <v8.h>

namespace WebCore {

class ExecutionContext;

// ScriptPromiseResolver is a class for performing operations on Promise
// (resolve / reject) from C++ world.
// ScriptPromiseResolver holds a PromiseResolver.
// Here is a typical usage:
//  1. Create a ScriptPromiseResolver.
//  2. Pass the associated promise object to a JavaScript program
//     (such as XMLHttpRequest return value).
//  3. Call resolve or reject when the operation completes or
//     the operation fails respectively.
//
// Most methods including constructors must be called within a v8 context.
// To use ScriptPromiseResolver out of a v8 context the caller must
// enter a v8 context. Please use ScriptPromiseResolverWithContext
// in such cases.
//
// To prevent memory leaks, you should release the reference manually
// by calling resolve or reject.
// Destroying the object will also release the reference.
// Note that ScriptPromiseResolver::promise returns an empty value when the
// resolver is already resolved or rejected. If you want to resolve a resolver
// immediately and return the associated promise, you should get the promise
// before resolving.
class ScriptPromiseResolver : public RefCounted<ScriptPromiseResolver> {
    WTF_MAKE_NONCOPYABLE(ScriptPromiseResolver);
public:
    static PassRefPtr<ScriptPromiseResolver> create(ScriptState*);

    // A ScriptPromiseResolver should be resolved / rejected before
    // its destruction.
    // A ScriptPromiseResolver can be destructed safely without
    // entering a v8 context.
    ~ScriptPromiseResolver();

    // Returns the underlying Promise.
    // Note that the underlying Promise is cleared when |resolve| or |reject|
    // is called.
    ScriptPromise promise();

    template<typename T>
    void resolve(const T& value, ExecutionContext* executionContext)
    {
        ASSERT(m_scriptState->isolate()->InContext());
        // You should use ScriptPromiseResolverWithContext when you want
        // to resolve a Promise in a non-original V8 context.
        return resolve(value);
    }
    template<typename T>
    void reject(const T& value, ExecutionContext* executionContext)
    {
        ASSERT(m_scriptState->isolate()->InContext());
        // You should use ScriptPromiseResolverWithContext when you want
        // to reject a Promise in a non-original V8 context.
        return reject(value);
    }
    template<typename T>
    void resolve(const T& value)
    {
        ASSERT(m_scriptState->isolate()->InContext());
        resolve(toV8Value(value));
    }
    template<typename T>
    void reject(const T& value)
    {
        ASSERT(m_scriptState->isolate()->InContext());
        reject(toV8Value(value));
    }
    template<typename T> void resolve(const T& value, v8::Handle<v8::Object> creationContext)
    {
        ASSERT(m_scriptState->isolate()->InContext());
        resolve(toV8Value(value, creationContext));
    }
    template<typename T> void reject(const T& value, v8::Handle<v8::Object> creationContext)
    {
        ASSERT(m_scriptState->isolate()->InContext());
        reject(toV8Value(value, creationContext));
    }

    v8::Isolate* isolate() const { return m_scriptState->isolate(); }

    void resolve(v8::Handle<v8::Value>);
    void reject(v8::Handle<v8::Value>);

private:
    template<typename T>
    v8::Handle<v8::Value> toV8Value(const T& value, v8::Handle<v8::Object> creationContext)
    {
        return ToV8Value<ScriptPromiseResolver, v8::Handle<v8::Object> >::toV8Value(value, creationContext, m_scriptState->isolate());
    }

    template<typename T>
    v8::Handle<v8::Value> toV8Value(const T& value)
    {
        return ToV8Value<ScriptPromiseResolver, v8::Handle<v8::Object> >::toV8Value(value, m_scriptState->context()->Global(), m_scriptState->isolate());
    }

    explicit ScriptPromiseResolver(ScriptState*);

    // FIXME: Remove this once ScriptValue::scriptState() becomes available.
    RefPtr<ScriptState> m_scriptState;
    ScriptValue m_resolver;
};

} // namespace WebCore


#endif // ScriptPromiseResolver_h
