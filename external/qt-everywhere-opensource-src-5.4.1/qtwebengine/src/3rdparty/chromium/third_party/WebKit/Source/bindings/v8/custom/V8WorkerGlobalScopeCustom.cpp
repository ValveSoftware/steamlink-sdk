/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "bindings/core/v8/V8WorkerGlobalScope.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScheduledAction.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8WorkerGlobalScopeEventListener.h"
#include "bindings/v8/WorkerScriptController.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/frame/DOMTimer.h"
#include "core/frame/DOMWindowTimers.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/websockets/WebSocket.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

static void setTimeoutOrInterval(const v8::FunctionCallbackInfo<v8::Value>& info, bool singleShot)
{
    WorkerGlobalScope* workerGlobalScope = V8WorkerGlobalScope::toNative(info.Holder());
    ASSERT(workerGlobalScope);

    int argumentCount = info.Length();
    if (argumentCount < 1)
        return;

    v8::Handle<v8::Value> function = info[0];

    WorkerScriptController* script = workerGlobalScope->script();
    if (!script)
        return;

    ScriptState* scriptState = ScriptState::current(info.GetIsolate());
    OwnPtr<ScheduledAction> action;
    if (function->IsString()) {
        if (ContentSecurityPolicy* policy = workerGlobalScope->contentSecurityPolicy()) {
            if (!policy->allowEval()) {
                v8SetReturnValue(info, 0);
                return;
            }
        }
        action = adoptPtr(new ScheduledAction(scriptState, toCoreString(function.As<v8::String>()), workerGlobalScope->url(), info.GetIsolate()));
    } else if (function->IsFunction()) {
        size_t paramCount = argumentCount >= 2 ? argumentCount - 2 : 0;
        OwnPtr<v8::Local<v8::Value>[]> params;
        if (paramCount > 0) {
            params = adoptArrayPtr(new v8::Local<v8::Value>[paramCount]);
            for (size_t i = 0; i < paramCount; ++i)
                params[i] = info[i+2];
        }
        // ScheduledAction takes ownership of actual params and releases them in its destructor.
        action = adoptPtr(new ScheduledAction(scriptState, v8::Handle<v8::Function>::Cast(function), paramCount, params.get(), info.GetIsolate()));
    } else
        return;

    int32_t timeout = argumentCount >= 2 ? info[1]->Int32Value() : 0;
    int timerId;
    if (singleShot)
        timerId = DOMWindowTimers::setTimeout(*workerGlobalScope, action.release(), timeout);
    else
        timerId = DOMWindowTimers::setInterval(*workerGlobalScope, action.release(), timeout);

    v8SetReturnValue(info, timerId);
}

void V8WorkerGlobalScope::setTimeoutMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    return setTimeoutOrInterval(info, true);
}

void V8WorkerGlobalScope::setIntervalMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    return setTimeoutOrInterval(info, false);
}

v8::Handle<v8::Value> toV8(WorkerGlobalScope* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    // Notice that we explicitly ignore creationContext because the WorkerGlobalScope is its own creationContext.

    if (!impl)
        return v8::Null(isolate);

    WorkerScriptController* script = impl->script();
    if (!script)
        return v8::Null(isolate);

    v8::Handle<v8::Object> global = script->context()->Global();
    ASSERT(!global.IsEmpty());
    return global;
}

} // namespace WebCore
