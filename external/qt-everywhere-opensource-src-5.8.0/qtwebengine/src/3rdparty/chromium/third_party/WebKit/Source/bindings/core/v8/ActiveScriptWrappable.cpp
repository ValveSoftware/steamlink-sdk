// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ActiveScriptWrappable.h"

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/ScriptWrappableVisitor.h"
#include "bindings/core/v8/V8PerIsolateData.h"

namespace blink {

ActiveScriptWrappable::ActiveScriptWrappable(ScriptWrappable* self)
    : m_scriptWrappable(self)
{
    ASSERT(ThreadState::current());
    v8::Isolate* isolate = ThreadState::current()->isolate();
    V8PerIsolateData* isolateData = V8PerIsolateData::from(isolate);
    isolateData->addActiveScriptWrappable(this);
}

void ActiveScriptWrappable::traceActiveScriptWrappables(v8::Isolate* isolate, ScriptWrappableVisitor* visitor)
{
    V8PerIsolateData* isolateData = V8PerIsolateData::from(isolate);
    auto activeScriptWrappables = isolateData->activeScriptWrappables();
    if (!activeScriptWrappables) {
        return;
    }

    for (auto activeWrappable : *activeScriptWrappables) {
        if (!activeWrappable->hasPendingActivity()) {
            continue;
        }

        auto scriptWrappable = activeWrappable->toScriptWrappable();
        auto wrapperTypeInfo = const_cast<WrapperTypeInfo*>(scriptWrappable->wrapperTypeInfo());
        visitor->RegisterV8Reference(std::make_pair(wrapperTypeInfo, scriptWrappable));
    }
}

} // namespace blink
