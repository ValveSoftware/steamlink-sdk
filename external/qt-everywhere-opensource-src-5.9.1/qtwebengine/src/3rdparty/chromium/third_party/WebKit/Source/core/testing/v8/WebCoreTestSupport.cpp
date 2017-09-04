/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/testing/v8/WebCoreTestSupport.h"

#include "bindings/core/v8/ConditionalFeatures.h"
#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8OriginTrialsTest.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/testing/InternalSettings.h"
#include "core/testing/Internals.h"
#include "core/testing/WorkerInternals.h"

using namespace blink;

namespace WebCoreTestSupport {

namespace {

blink::InstallConditionalFeaturesFunction
    s_originalInstallConditionalFeaturesFunction = nullptr;

v8::Local<v8::Value> createInternalsObject(v8::Local<v8::Context> context) {
  ScriptState* scriptState = ScriptState::from(context);
  v8::Local<v8::Object> global = scriptState->context()->Global();
  ExecutionContext* executionContext = scriptState->getExecutionContext();
  if (executionContext->isDocument()) {
    return toV8(Internals::create(executionContext), global,
                scriptState->isolate());
  }
  if (executionContext->isWorkerGlobalScope())
    return toV8(WorkerInternals::create(), global, scriptState->isolate());
  return v8::Local<v8::Value>();
}
}

void injectInternalsObject(v8::Local<v8::Context> context) {
  // Set conditional features installation function to
  // |installConditionalFeaturesForTests|
  if (!s_originalInstallConditionalFeaturesFunction) {
    s_originalInstallConditionalFeaturesFunction =
        setInstallConditionalFeaturesFunction(
            installConditionalFeaturesForTests);
  }

  ScriptState* scriptState = ScriptState::from(context);
  ScriptState::Scope scope(scriptState);
  v8::Local<v8::Object> global = scriptState->context()->Global();
  v8::Local<v8::Value> internals = createInternalsObject(context);
  if (internals.IsEmpty())
    return;

  global
      ->Set(scriptState->context(),
            v8AtomicString(scriptState->isolate(), "internals"), internals)
      .ToChecked();
}

void installConditionalFeaturesForTests(
    const WrapperTypeInfo* type,
    const blink::ScriptState* scriptState,
    v8::Local<v8::Object> prototypeObject,
    v8::Local<v8::Function> interfaceObject) {
  (*s_originalInstallConditionalFeaturesFunction)(
      type, scriptState, prototypeObject, interfaceObject);

  ExecutionContext* executionContext = scriptState->getExecutionContext();
  OriginTrialContext* originTrialContext = OriginTrialContext::from(
      executionContext, OriginTrialContext::DontCreateIfNotExists);

  if (type == &V8OriginTrialsTest::wrapperTypeInfo) {
    if (originTrialContext && originTrialContext->isTrialEnabled("Frobulate")) {
      V8OriginTrialsTest::installOriginTrialsSampleAPI(
          scriptState->isolate(), scriptState->world(), v8::Local<v8::Object>(),
          prototypeObject, interfaceObject);
    }
  }
}

void resetInternalsObject(v8::Local<v8::Context> context) {
  // This can happen if JavaScript is disabled in the main frame.
  if (context.IsEmpty())
    return;

  ScriptState* scriptState = ScriptState::from(context);
  ScriptState::Scope scope(scriptState);
  Document* document = toDocument(scriptState->getExecutionContext());
  ASSERT(document);
  LocalFrame* frame = document->frame();
  // Should the document have been detached, the page is assumed being destroyed
  // (=> no reset required.)
  if (!frame)
    return;
  Page* page = frame->page();
  ASSERT(page);
  Internals::resetToConsistentState(page);
  InternalSettings::from(*page)->resetToConsistentState();
}

}  // namespace WebCoreTestSupport
