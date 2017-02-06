/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8PerIsolateData_h
#define V8PerIsolateData_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptWrappableVisitor.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "bindings/core/v8/WrapperTypeInfo.h"
#include "core/CoreExport.h"
#include "gin/public/isolate_holder.h"
#include "gin/public/v8_idle_task_runner.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"
#include <memory>
#include <v8.h>

namespace blink {

class ActiveScriptWrappable;
class DOMDataStore;
class StringCache;
class ThreadDebugger;
class V8PrivateProperty;
struct WrapperTypeInfo;

typedef WTF::Vector<DOMDataStore*> DOMDataStoreList;

class CORE_EXPORT V8PerIsolateData {
    USING_FAST_MALLOC(V8PerIsolateData);
    WTF_MAKE_NONCOPYABLE(V8PerIsolateData);
public:
    class EndOfScopeTask {
        USING_FAST_MALLOC(EndOfScopeTask);
    public:
        virtual ~EndOfScopeTask() { }
        virtual void run() = 0;
    };

    // Disables the UseCounter.
    // UseCounter depends on the current context, but it's not available during
    // the initialization of v8::Context and the global object.  So we need to
    // disable the UseCounter while the initialization of the context and global
    // object.
    // TODO(yukishiino): Come up with an idea to remove this hack.
    class UseCounterDisabledScope {
        STACK_ALLOCATED();
    public:
        explicit UseCounterDisabledScope(V8PerIsolateData* perIsolateData)
            : m_perIsolateData(perIsolateData)
            , m_originalUseCounterDisabled(m_perIsolateData->m_useCounterDisabled)
        {
            m_perIsolateData->m_useCounterDisabled = true;
        }
        ~UseCounterDisabledScope()
        {
            m_perIsolateData->m_useCounterDisabled = m_originalUseCounterDisabled;
        }

    private:
        V8PerIsolateData* m_perIsolateData;
        const bool m_originalUseCounterDisabled;
    };

    static v8::Isolate* initialize();

    static V8PerIsolateData* from(v8::Isolate* isolate)
    {
        ASSERT(isolate);
        ASSERT(isolate->GetData(gin::kEmbedderBlink));
        return static_cast<V8PerIsolateData*>(isolate->GetData(gin::kEmbedderBlink));
    }

    static void willBeDestroyed(v8::Isolate*);
    static void destroy(v8::Isolate*);
    static v8::Isolate* mainThreadIsolate();

    static void enableIdleTasks(v8::Isolate*, std::unique_ptr<gin::V8IdleTaskRunner>);

    v8::Isolate* isolate() { return m_isolateHolder->isolate(); }

    StringCache* getStringCache() { return m_stringCache.get(); }

    v8::Persistent<v8::Value>& ensureLiveRoot();

    bool isHandlingRecursionLevelError() const { return m_isHandlingRecursionLevelError; }
    void setIsHandlingRecursionLevelError(bool value) { m_isHandlingRecursionLevelError = value; }

    bool isReportingException() const { return m_isReportingException; }
    void setReportingException(bool value) { m_isReportingException = value; }

    V8HiddenValue* hiddenValue() { return m_hiddenValue.get(); }
    V8PrivateProperty* privateProperty() { return m_privateProperty.get(); }

    // Accessors to the cache of interface templates.
    v8::Local<v8::FunctionTemplate> findInterfaceTemplate(const DOMWrapperWorld&, const void* key);
    void setInterfaceTemplate(const DOMWrapperWorld&, const void* key, v8::Local<v8::FunctionTemplate>);

    // Accessor to the cache of cross-origin accessible operation's templates.
    // Created templates get automatically cached.
    v8::Local<v8::FunctionTemplate> findOrCreateOperationTemplate(const DOMWrapperWorld&, const void* key, v8::FunctionCallback, v8::Local<v8::Value> data, v8::Local<v8::Signature>, int length);

    bool hasInstance(const WrapperTypeInfo* untrusted, v8::Local<v8::Value>);
    v8::Local<v8::Object> findInstanceInPrototypeChain(const WrapperTypeInfo*, v8::Local<v8::Value>);

    v8::Local<v8::Context> ensureScriptRegexpContext();
    void clearScriptRegexpContext();

    // EndOfScopeTasks are run when control is returning
    // to C++ from script, after executing a script task (e.g. callback,
    // event) or microtasks (e.g. promise). This is explicitly needed for
    // Indexed DB transactions per spec, but should in general be avoided.
    void addEndOfScopeTask(std::unique_ptr<EndOfScopeTask>);
    void runEndOfScopeTasks();
    void clearEndOfScopeTasks();

    void setThreadDebugger(std::unique_ptr<ThreadDebugger>);
    ThreadDebugger* threadDebugger();

    using ActiveScriptWrappableSet = HeapHashSet<WeakMember<ActiveScriptWrappable>>;
    void addActiveScriptWrappable(ActiveScriptWrappable*);
    const ActiveScriptWrappableSet* activeScriptWrappables() const { return m_activeScriptWrappables.get(); }

    void setScriptWrappableVisitor(std::unique_ptr<ScriptWrappableVisitor> visitor) { m_scriptWrappableVisitor = std::move(visitor); }
    ScriptWrappableVisitor* scriptWrappableVisitor() { return m_scriptWrappableVisitor.get(); }

private:
    V8PerIsolateData();
    ~V8PerIsolateData();

    static void useCounterCallback(v8::Isolate*, v8::Isolate::UseCounterFeature);

    typedef HashMap<const void*, v8::Eternal<v8::FunctionTemplate>> V8FunctionTemplateMap;
    V8FunctionTemplateMap& selectInterfaceTemplateMap(const DOMWrapperWorld&);
    V8FunctionTemplateMap& selectOperationTemplateMap(const DOMWrapperWorld&);
    bool hasInstance(const WrapperTypeInfo* untrusted, v8::Local<v8::Value>, V8FunctionTemplateMap&);
    v8::Local<v8::Object> findInstanceInPrototypeChain(const WrapperTypeInfo*, v8::Local<v8::Value>, V8FunctionTemplateMap&);

    std::unique_ptr<gin::IsolateHolder> m_isolateHolder;

    // m_interfaceTemplateMapFor{,Non}MainWorld holds function templates for
    // the inerface objects.
    V8FunctionTemplateMap m_interfaceTemplateMapForMainWorld;
    V8FunctionTemplateMap m_interfaceTemplateMapForNonMainWorld;
    // m_operationTemplateMapFor{,Non}MainWorld holds function templates for
    // the cross-origin accessible DOM operations.
    V8FunctionTemplateMap m_operationTemplateMapForMainWorld;
    V8FunctionTemplateMap m_operationTemplateMapForNonMainWorld;

    std::unique_ptr<StringCache> m_stringCache;
    std::unique_ptr<V8HiddenValue> m_hiddenValue;
    std::unique_ptr<V8PrivateProperty> m_privateProperty;
    ScopedPersistent<v8::Value> m_liveRoot;
    RefPtr<ScriptState> m_scriptRegexpScriptState;

    bool m_constructorMode;
    friend class ConstructorMode;

    bool m_useCounterDisabled;
    friend class UseCounterDisabledScope;

    bool m_isHandlingRecursionLevelError;
    bool m_isReportingException;

    Vector<std::unique_ptr<EndOfScopeTask>> m_endOfScopeTasks;
    std::unique_ptr<ThreadDebugger> m_threadDebugger;

    Persistent<ActiveScriptWrappableSet> m_activeScriptWrappables;
    std::unique_ptr<ScriptWrappableVisitor> m_scriptWrappableVisitor;
};

} // namespace blink

#endif // V8PerIsolateData_h
