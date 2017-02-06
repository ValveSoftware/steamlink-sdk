// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/worklet/Worklet.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/workers/WorkletGlobalScopeProxy.h"

namespace blink {

Worklet::Worklet(ExecutionContext* executionContext)
    : ActiveDOMObject(executionContext)
{
}

ScriptPromise Worklet::import(ScriptState* scriptState, const String& url)
{
    KURL scriptURL = getExecutionContext()->completeURL(url);
    if (!scriptURL.isValid()) {
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(SyntaxError, "'" + url + "' is not a valid URL."));
    }

    // TODO(ikilpatrick): Perform upfront CSP checks once we decide on a
    // CSP-policy for worklets.

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    m_resolvers.append(resolver);

    ScriptPromise promise = resolver->promise();

    // TODO(ikilpatrick): WorkerScriptLoader will need to be extended to allow
    // module loading support. For now just fetch a 'classic' script.

    // NOTE: WorkerScriptLoader may synchronously invoke its callbacks
    // (resolving the promise) before we return it.
    m_scriptLoaders.append(WorkerScriptLoader::create());
    m_scriptLoaders.last()->loadAsynchronously(*getExecutionContext(), scriptURL, DenyCrossOriginRequests,
        getExecutionContext()->securityContext().addressSpace(),
        bind(&Worklet::onResponse, wrapPersistent(this), WTF::unretained(m_scriptLoaders.last().get())),
        bind(&Worklet::onFinished, wrapPersistent(this), WTF::unretained(m_scriptLoaders.last().get()), wrapPersistent(resolver)));

    return promise;
}

void Worklet::onResponse(WorkerScriptLoader* scriptLoader)
{
    InspectorInstrumentation::didReceiveScriptResponse(getExecutionContext(), scriptLoader->identifier());
}

void Worklet::onFinished(WorkerScriptLoader* scriptLoader, ScriptPromiseResolver* resolver)
{
    if (scriptLoader->failed()) {
        resolver->reject(DOMException::create(NetworkError));
    } else {
        // TODO(ikilpatrick): Worklets don't have the same error behaviour
        // as workers, etc. For a SyntaxError we should reject, however if
        // the script throws a normal error, resolve. For now just resolve.
        workletGlobalScopeProxy()->evaluateScript(scriptLoader->script(), scriptLoader->url());
        InspectorInstrumentation::scriptImported(getExecutionContext(), scriptLoader->identifier(), scriptLoader->script());
        resolver->resolve();
    }

    size_t index = m_scriptLoaders.find(scriptLoader);

    ASSERT(index != kNotFound);
    ASSERT(m_resolvers[index] == resolver);

    m_scriptLoaders.remove(index);
    m_resolvers.remove(index);
}

void Worklet::stop()
{
    workletGlobalScopeProxy()->terminateWorkletGlobalScope();

    for (auto scriptLoader : m_scriptLoaders) {
        scriptLoader->cancel();
    }
}

DEFINE_TRACE(Worklet)
{
    visitor->trace(m_resolvers);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
