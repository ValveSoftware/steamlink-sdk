// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/PaintWorklet.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/frame/LocalFrame.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/csspaint/CSSPaintDefinition.h"
#include "modules/csspaint/PaintWorkletGlobalScope.h"
#include "modules/csspaint/WindowPaintWorklet.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

namespace {

static void clearHandle(const v8::WeakCallbackInfo<ScopedPersistent<v8::Function>>& data)
{
    data.GetParameter()->clear();
}

} // namespace

class PaintWorkletTest : public testing::Test {
public:
    PaintWorkletTest()
        : m_page(DummyPageHolder::create())
    {
    }

    PaintWorklet* paintWorklet()
    {
        return WindowPaintWorklet::from(*m_page->frame().localDOMWindow()).paintWorklet(&m_page->document());
    }

protected:
    std::unique_ptr<DummyPageHolder> m_page;
};

TEST_F(PaintWorkletTest, GarbageCollectionOfCSSPaintDefinition)
{
    PaintWorkletGlobalScope* globalScope = paintWorklet()->workletGlobalScopeProxy();
    globalScope->scriptController()->evaluate(ScriptSourceCode("registerPaint('foo', class { paint() { } });"));

    CSSPaintDefinition* definition = globalScope->findDefinition("foo");
    ASSERT(definition);

    v8::Isolate* isolate = globalScope->scriptController()->getScriptState()->isolate();
    ASSERT(isolate);

    // Set our ScopedPersistent to the paint function, and make weak.
    ScopedPersistent<v8::Function> handle;
    {
        v8::HandleScope handleScope(isolate);
        handle.set(isolate, definition->paintFunctionForTesting(isolate));
        handle.setWeak(&handle, clearHandle);
    }
    ASSERT(!handle.isEmpty());
    ASSERT(handle.isWeak());

    // Run a GC, persistent shouldn't have been collected yet.
    ThreadHeap::collectAllGarbage();
    V8GCController::collectAllGarbageForTesting(isolate);
    ASSERT(!handle.isEmpty());

    // Delete the page & associated objects.
    m_page.reset();

    // Run a GC, the persistent should have been collected.
    ThreadHeap::collectAllGarbage();
    V8GCController::collectAllGarbageForTesting(isolate);
    ASSERT(handle.isEmpty());
}

} // naespace blink
