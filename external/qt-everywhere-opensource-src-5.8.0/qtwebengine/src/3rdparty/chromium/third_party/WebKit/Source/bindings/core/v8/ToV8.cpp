// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ToV8.h"

#include "bindings/core/v8/WindowProxy.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/events/EventTarget.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/Frame.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"

namespace blink {

v8::Local<v8::Value> toV8(DOMWindow* window, v8::Local<v8::Object> creationContext, v8::Isolate* isolate)
{
    // Notice that we explicitly ignore creationContext because the DOMWindow
    // has its own creationContext.

    if (UNLIKELY(!window))
        return v8::Null(isolate);

    // TODO(yukishiino): There must be no case to return undefined.
    // 'window', 'frames' and 'self' attributes in Window interface return
    // the WindowProxy object of the browsing context, which never be undefined.
    // 'top' and 'parent' attributes return the same when detached.  Therefore,
    // there must be no case to return undefined.
    // See http://crbug.com/621730 and http://crbug.com/621577 .
    if (!window->isCurrentlyDisplayedInFrame())
        return v8Undefined();

    Frame* frame = window->frame();
    return frame->windowProxy(DOMWrapperWorld::current(isolate))->globalIfNotDetached();
}

v8::Local<v8::Value> toV8(EventTarget* impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (UNLIKELY(!impl))
        return v8::Null(isolate);

    if (impl->interfaceName() == EventTargetNames::DOMWindow)
        return toV8(static_cast<DOMWindow*>(impl), creationContext, isolate);
    return toV8(static_cast<ScriptWrappable*>(impl), creationContext, isolate);
}

v8::Local<v8::Value> toV8(WorkerOrWorkletGlobalScope* impl, v8::Local<v8::Object> creationContext, v8::Isolate* isolate)
{
    // Notice that we explicitly ignore creationContext because the
    // WorkerOrWorkletGlobalScope has its own creationContext.

    if (UNLIKELY(!impl))
        return v8::Null(isolate);

    WorkerOrWorkletScriptController* scriptController = impl->scriptController();
    if (!scriptController)
        return v8::Null(isolate);

    v8::Local<v8::Object> global = scriptController->context()->Global();
    ASSERT(!global.IsEmpty());
    return global;
}

} // namespace blink
