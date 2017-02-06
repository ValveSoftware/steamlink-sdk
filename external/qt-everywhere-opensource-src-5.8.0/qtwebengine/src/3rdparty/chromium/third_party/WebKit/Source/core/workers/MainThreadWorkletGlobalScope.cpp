// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/MainThreadWorkletGlobalScope.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/frame/FrameConsole.h"

namespace blink {

MainThreadWorkletGlobalScope::MainThreadWorkletGlobalScope(LocalFrame* frame, const KURL& url, const String& userAgent, PassRefPtr<SecurityOrigin> securityOrigin, v8::Isolate* isolate)
    : WorkletGlobalScope(url, userAgent, securityOrigin, isolate)
    , LocalFrameLifecycleObserver(frame)
{
}

MainThreadWorkletGlobalScope::~MainThreadWorkletGlobalScope()
{
}

void MainThreadWorkletGlobalScope::evaluateScript(const String& source, const KURL& scriptURL)
{
    scriptController()->evaluate(ScriptSourceCode(source, scriptURL));
}

void MainThreadWorkletGlobalScope::terminateWorkletGlobalScope()
{
    dispose();
}

void MainThreadWorkletGlobalScope::addConsoleMessage(ConsoleMessage* consoleMessage)
{
    frame()->console().addMessage(consoleMessage);
}

} // namespace blink
