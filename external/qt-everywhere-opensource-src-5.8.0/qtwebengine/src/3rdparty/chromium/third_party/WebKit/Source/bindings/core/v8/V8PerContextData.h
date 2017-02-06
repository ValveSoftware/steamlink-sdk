/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef V8PerContextData_h
#define V8PerContextData_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/V0CustomElementBinding.h"
#include "bindings/core/v8/V8GlobalValueMap.h"
#include "bindings/core/v8/WrapperTypeInfo.h"
#include "core/CoreExport.h"
#include "gin/public/context_holder.h"
#include "gin/public/gin_embedders.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/AtomicStringHash.h"
#include <memory>
#include <v8.h>

namespace blink {

class V8DOMActivityLogger;
class V8PerContextData;

enum V8ContextEmbedderDataField {
    v8ContextPerContextDataIndex = static_cast<int>(gin::kPerContextDataStartIndex + gin::kEmbedderBlink),
};

class CORE_EXPORT V8PerContextData final {
    USING_FAST_MALLOC(V8PerContextData);
    WTF_MAKE_NONCOPYABLE(V8PerContextData);
public:
    static std::unique_ptr<V8PerContextData> create(v8::Local<v8::Context>);

    static V8PerContextData* from(v8::Local<v8::Context>);

    ~V8PerContextData();

    v8::Local<v8::Context> context() { return m_context.newLocal(m_isolate); }

    // To create JS Wrapper objects, we create a cache of a 'boiler plate'
    // object, and then simply Clone that object each time we need a new one.
    // This is faster than going through the full object creation process.
    v8::Local<v8::Object> createWrapperFromCache(const WrapperTypeInfo* type)
    {
        v8::Local<v8::Object> boilerplate = m_wrapperBoilerplates.Get(type);
        return !boilerplate.IsEmpty() ? boilerplate->Clone() : createWrapperFromCacheSlowCase(type);
    }

    v8::Local<v8::Function> constructorForType(const WrapperTypeInfo* type)
    {
        v8::Local<v8::Function> interfaceObject = m_constructorMap.Get(type);
        return (!interfaceObject.IsEmpty()) ? interfaceObject : constructorForTypeSlowCase(type);
    }

    v8::Local<v8::Object> prototypeForType(const WrapperTypeInfo*);

    void addCustomElementBinding(std::unique_ptr<V0CustomElementBinding>);

    V8DOMActivityLogger* activityLogger() const { return m_activityLogger; }
    void setActivityLogger(V8DOMActivityLogger* activityLogger) { m_activityLogger = activityLogger; }

    v8::Local<v8::Value> compiledPrivateScript(String);
    void setCompiledPrivateScript(String, v8::Local<v8::Value>);

private:
    V8PerContextData(v8::Local<v8::Context>);

    v8::Local<v8::Object> createWrapperFromCacheSlowCase(const WrapperTypeInfo*);
    v8::Local<v8::Function> constructorForTypeSlowCase(const WrapperTypeInfo*);

    v8::Isolate* m_isolate;

    // For each possible type of wrapper, we keep a boilerplate object.
    // The boilerplate is used to create additional wrappers of the same type.
    typedef V8GlobalValueMap<const WrapperTypeInfo*, v8::Object, v8::kNotWeak> WrapperBoilerplateMap;
    WrapperBoilerplateMap m_wrapperBoilerplates;

    typedef V8GlobalValueMap<const WrapperTypeInfo*, v8::Function, v8::kNotWeak> ConstructorMap;
    ConstructorMap m_constructorMap;

    std::unique_ptr<gin::ContextHolder> m_contextHolder;

    ScopedPersistent<v8::Context> m_context;
    ScopedPersistent<v8::Value> m_errorPrototype;

    typedef Vector<std::unique_ptr<V0CustomElementBinding>> V0CustomElementBindingList;
    V0CustomElementBindingList m_customElementBindings;

    // This is owned by a static hash map in V8DOMActivityLogger.
    V8DOMActivityLogger* m_activityLogger;

    V8GlobalValueMap<String, v8::Value, v8::kNotWeak> m_compiledPrivateScript;
};

} // namespace blink

#endif // V8PerContextData_h
