// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptWrappableVisitorVerifier_h
#define ScriptWrappableVisitorVerifier_h

#include "bindings/core/v8/ScriptWrappableVisitor.h"
#include "bindings/core/v8/V8AbstractEventListener.h"
#include "core/dom/DocumentStyleSheetCollection.h"
#include "core/dom/ElementRareData.h"
#include "core/dom/NodeListsNodeData.h"
#include "core/dom/NodeRareData.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/html/imports/HTMLImportsController.h"

namespace blink {

class ScriptWrappableVisitorVerifier : public WrapperVisitor {
public:
    void dispatchTraceWrappers(const ScriptWrappable* t) const override { t->traceWrappers(this); }
#define DECLARE_DISPATCH_TRACE_WRAPPERS(className)                   \
    void dispatchTraceWrappers(const className* t) const override { t->traceWrappers(this); }

    WRAPPER_VISITOR_SPECIAL_CLASSES(DECLARE_DISPATCH_TRACE_WRAPPERS);

#undef DECLARE_DISPATCH_TRACE_WRAPPERS
    void dispatchTraceWrappers(const void*) const override {}

    void traceWrappers(const ScopedPersistent<v8::Value>*) const override {}
    void traceWrappers(const ScopedPersistent<v8::Object>*) const override {}
    void markWrapper(const v8::PersistentBase<v8::Object>*) const override {}

    void pushToMarkingDeque(
        void (*traceWrappersCallback)(const WrapperVisitor*, const void*),
        HeapObjectHeader* (*heapObjectHeaderCallback)(const void*),
        const void* object) const override
    {
        if (!heapObjectHeaderCallback(object)->isWrapperHeaderMarked()) {
            /*
             * If this branch is hit, it means that a white (not discovered by
             * traceWrappers) object was assigned as a member to a black object
             * (already processed by traceWrappers). Black object will not be
             * processed anymore so White object will remain undetected and
             * therefore its wrapper and all wrappers reachable from it would be
             * collected.
             *
             * Most often this means there is a write barrier missing somewhere.
             * Check backtrace to see which classes are causing this and review
             * all the places where white class is set to the black class.
             */
            NOTREACHED();
        }
        traceWrappersCallback(this, object);
    }

    bool markWrapperHeader(HeapObjectHeader* header) const override
    {
        if (!m_visitedHeaders.contains(header)) {
            m_visitedHeaders.add(header);
            return true;
        }
        return false;
    }
    void markWrappersInAllWorlds(const ScriptWrappable*) const override {}
    void markWrappersInAllWorlds(const void*) const override {}
private:
    mutable WTF::HashSet<HeapObjectHeader*> m_visitedHeaders;
};

}
#endif
