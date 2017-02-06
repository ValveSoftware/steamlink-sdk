// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptWrappable.h"

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/ScriptWrappableVisitor.h"
#include "bindings/core/v8/V8DOMWrapper.h"

namespace blink {

struct SameSizeAsScriptWrappable {
    virtual ~SameSizeAsScriptWrappable() { }
    v8::Persistent<v8::Object> m_mainWorldWrapper;
};

static_assert(sizeof(ScriptWrappable) <= sizeof(SameSizeAsScriptWrappable), "ScriptWrappable should stay small");

v8::Local<v8::Object> ScriptWrappable::wrap(v8::Isolate* isolate, v8::Local<v8::Object> creationContext)
{
    const WrapperTypeInfo* wrapperTypeInfo = this->wrapperTypeInfo();

    ASSERT(!DOMDataStore::containsWrapper(this, isolate));

    v8::Local<v8::Object> wrapper = V8DOMWrapper::createWrapper(isolate, creationContext, wrapperTypeInfo);
    if (UNLIKELY(wrapper.IsEmpty()))
        return wrapper;

    wrapperTypeInfo->installConditionallyEnabledProperties(wrapper, isolate);
    return associateWithWrapper(isolate, wrapperTypeInfo, wrapper);
}

v8::Local<v8::Object> ScriptWrappable::associateWithWrapper(v8::Isolate* isolate, const WrapperTypeInfo* wrapperTypeInfo, v8::Local<v8::Object> wrapper)
{
    return V8DOMWrapper::associateObjectWithWrapper(isolate, this, wrapperTypeInfo, wrapper);
}

void ScriptWrappable::markWrapper(const WrapperVisitor* visitor) const
{
    if (containsWrapper())
        visitor->markWrapper(&m_mainWorldWrapper);
}

} // namespace blink
