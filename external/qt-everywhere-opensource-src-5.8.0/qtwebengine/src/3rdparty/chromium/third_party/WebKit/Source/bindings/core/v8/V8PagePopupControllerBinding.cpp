// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8PagePopupControllerBinding.h"

#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8Window.h"
#include "core/dom/ContextFeatures.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/page/PagePopupController.h"
#include "core/page/PagePopupSupplement.h"
#include "platform/TraceEvent.h"

namespace blink {

namespace {

void pagePopupControllerAttributeGetter(const v8::PropertyCallbackInfo<v8::Value>& info)
{
    v8::Local<v8::Object> holder = info.Holder();
    DOMWindow* impl = V8Window::toImpl(holder);
    PagePopupController* cppValue = PagePopupSupplement::pagePopupController(*toLocalDOMWindow(impl)->frame());
    v8SetReturnValue(info, toV8(cppValue, holder, info.GetIsolate()));
}

void pagePopupControllerAttributeGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    pagePopupControllerAttributeGetter(info);
}

} // namespace

void V8PagePopupControllerBinding::installPagePopupController(v8::Local<v8::Context> context, v8::Local<v8::Object> windowWrapper)
{
    ExecutionContext* executionContext = toExecutionContext(windowWrapper->CreationContext());
    if (!(executionContext && executionContext->isDocument()
        && ContextFeatures::pagePopupEnabled(toDocument(executionContext))))
        return;

    windowWrapper->SetAccessor(
        context,
        v8AtomicString(context->GetIsolate(), "pagePopupController"),
        pagePopupControllerAttributeGetterCallback);
}

} // namespace blink
