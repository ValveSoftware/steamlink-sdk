// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Worklet_h
#define Worklet_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/workers/WorkerScriptLoader.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExecutionContext;
class ScriptPromiseResolver;
class WorkerScriptLoader;
class WorkletGlobalScopeProxy;

class MODULES_EXPORT Worklet : public GarbageCollectedFinalized<Worklet>, public ScriptWrappable, public ActiveDOMObject {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(Worklet);
    WTF_MAKE_NONCOPYABLE(Worklet);
public:
    virtual WorkletGlobalScopeProxy* workletGlobalScopeProxy() const = 0;

    // Worklet
    ScriptPromise import(ScriptState*, const String& url);

    // ActiveDOMObject
    void stop() final;

    DECLARE_VIRTUAL_TRACE();

protected:
    // The ExecutionContext argument is the parent document of the Worklet. The
    // Worklet inherits the url and userAgent from the document.
    explicit Worklet(ExecutionContext*);

private:
    void onResponse(WorkerScriptLoader*);
    void onFinished(WorkerScriptLoader*, ScriptPromiseResolver*);

    Vector<RefPtr<WorkerScriptLoader>> m_scriptLoaders;
    HeapVector<Member<ScriptPromiseResolver>> m_resolvers;
};

} // namespace blink

#endif // Worklet_h
