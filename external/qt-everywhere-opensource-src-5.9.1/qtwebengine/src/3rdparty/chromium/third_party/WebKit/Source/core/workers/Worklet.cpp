// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/Worklet.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameFetchContext.h"
#include "core/workers/WorkletGlobalScopeProxy.h"
#include "core/workers/WorkletScriptLoader.h"

namespace blink {

Worklet::Worklet(LocalFrame* frame)
    : ActiveDOMObject(frame->document()),
      m_fetcher(frame->loader().documentLoader()->fetcher()) {}

ScriptPromise Worklet::import(ScriptState* scriptState, const String& url) {
  if (!isInitialized()) {
    initialize();
  }

  KURL scriptURL = getExecutionContext()->completeURL(url);
  if (!scriptURL.isValid()) {
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(SyntaxError, "'" + url + "' is not a valid URL."));
  }

  ResourceRequest resourceRequest(scriptURL);
  resourceRequest.setRequestContext(WebURLRequest::RequestContextScript);
  FetchRequest request(resourceRequest, FetchInitiatorTypeNames::internal);
  ScriptResource* resource = ScriptResource::fetch(request, fetcher());

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  if (resource) {
    WorkletScriptLoader* workletLoader =
        WorkletScriptLoader::create(resolver, this, resource);
    m_scriptLoaders.add(workletLoader);
  } else {
    resolver->reject(DOMException::create(NetworkError));
  }
  return promise;
}

void Worklet::notifyFinished(WorkletScriptLoader* scriptLoader) {
  workletGlobalScopeProxy()->evaluateScript(
      ScriptSourceCode(scriptLoader->resource()));
  m_scriptLoaders.remove(scriptLoader);
}

void Worklet::contextDestroyed() {
  if (isInitialized()) {
    workletGlobalScopeProxy()->terminateWorkletGlobalScope();
  }

  for (const auto& scriptLoader : m_scriptLoaders) {
    scriptLoader->cancel();
  }
}

DEFINE_TRACE(Worklet) {
  visitor->trace(m_fetcher);
  visitor->trace(m_scriptLoaders);
  ActiveDOMObject::trace(visitor);
}

}  // namespace blink
