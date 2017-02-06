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

#include "bindings/core/v8/V8GCController.h"

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/RetainedDOMInfo.h"
#include "bindings/core/v8/ScriptWrappableVisitor.h"
#include "bindings/core/v8/V8AbstractEventListener.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8Node.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/WrapperTypeInfo.h"
#include "core/dom/Attr.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "public/platform/BlameContext.h"
#include "public/platform/Platform.h"
#include "wtf/Vector.h"
#include "wtf/allocator/Partitions.h"
#include <algorithm>

namespace blink {

// FIXME: This should use opaque GC roots.
static void addReferencesForNodeWithEventListeners(v8::Isolate* isolate, Node* node, const v8::Persistent<v8::Object>& wrapper)
{
    ASSERT(node->hasEventListeners());

    EventListenerIterator iterator(node);
    while (EventListener* listener = iterator.nextListener()) {
        if (listener->type() != EventListener::JSEventListenerType)
            continue;
        V8AbstractEventListener* v8listener = static_cast<V8AbstractEventListener*>(listener);
        if (!v8listener->hasExistingListenerObject())
            continue;

        isolate->SetReference(wrapper, v8::Persistent<v8::Value>::Cast(v8listener->existingListenerObjectPersistentHandle()));
    }
}

Node* V8GCController::opaqueRootForGC(v8::Isolate*, Node* node)
{
    ASSERT(node);
    if (node->inShadowIncludingDocument()) {
        Document& document = node->document();
        if (HTMLImportsController* controller = document.importsController())
            return controller->master();
        return &document;
    }

    if (node->isAttributeNode()) {
        Node* ownerElement = toAttr(node)->ownerElement();
        if (!ownerElement)
            return node;
        node = ownerElement;
    }

    while (Node* parent = node->parentOrShadowHostOrTemplateHostNode())
        node = parent;

    return node;
}

class MinorGCUnmodifiedWrapperVisitor : public v8::PersistentHandleVisitor {
public:
    explicit MinorGCUnmodifiedWrapperVisitor(v8::Isolate* isolate)
        : m_isolate(isolate)
    { }

    void VisitPersistentHandle(v8::Persistent<v8::Value>* value, uint16_t classId) override
    {

        if (classId != WrapperTypeInfo::NodeClassId && classId != WrapperTypeInfo::ObjectClassId) {
            return;
        }

        // MinorGC does not collect objects because it may be expensive to
        // update references during minorGC
        if (classId == WrapperTypeInfo::ObjectClassId) {
            v8::Persistent<v8::Object>::Cast(*value).MarkActive();
            return;
        }

        v8::Local<v8::Object> wrapper = v8::Local<v8::Object>::New(m_isolate, v8::Persistent<v8::Object>::Cast(*value));
        ASSERT(V8DOMWrapper::hasInternalFieldsSet(wrapper));
        if (toWrapperTypeInfo(wrapper)->hasPendingActivity(wrapper)) {
            v8::Persistent<v8::Object>::Cast(*value).MarkActive();
            return;
        }

        if (classId == WrapperTypeInfo::NodeClassId) {
            ASSERT(V8Node::hasInstance(wrapper, m_isolate));
            Node* node = V8Node::toImpl(wrapper);
            if (node->hasEventListeners()) {
                v8::Persistent<v8::Object>::Cast(*value).MarkActive();
                return;
            }
            // FIXME: Remove the special handling for SVG elements.
            // We currently can't collect SVG Elements from minor gc, as we have
            // strong references from SVG property tear-offs keeping context SVG element alive.
            if (node->isSVGElement()) {
                v8::Persistent<v8::Object>::Cast(*value).MarkActive();
                return;
            }
        }
    }

private:
    v8::Isolate* m_isolate;
};

class MajorGCWrapperVisitor : public v8::PersistentHandleVisitor {
public:
    explicit MajorGCWrapperVisitor(v8::Isolate* isolate, bool constructRetainedObjectInfos)
        : m_isolate(isolate)
        , m_domObjectsWithPendingActivity(0)
        , m_liveRootGroupIdSet(false)
        , m_constructRetainedObjectInfos(constructRetainedObjectInfos)
    {
    }

    void VisitPersistentHandle(v8::Persistent<v8::Value>* value, uint16_t classId) override
    {
        if (classId != WrapperTypeInfo::NodeClassId && classId != WrapperTypeInfo::ObjectClassId)
            return;

        v8::Local<v8::Object> wrapper = v8::Local<v8::Object>::New(m_isolate, v8::Persistent<v8::Object>::Cast(*value));
        ASSERT(V8DOMWrapper::hasInternalFieldsSet(wrapper));

        const WrapperTypeInfo* type = toWrapperTypeInfo(wrapper);
        if (type->hasPendingActivity(wrapper)) {
            // If you hit this assert, you'll need to add a [DependentiLifetime]
            // extended attribute to the DOM interface. A DOM interface that
            // overrides hasPendingActivity must be marked as [DependentLifetime].
            RELEASE_ASSERT(!value->IsIndependent());
            m_isolate->SetObjectGroupId(*value, liveRootId());
            ++m_domObjectsWithPendingActivity;
        }

        if (value->IsIndependent())
            return;

        if (classId == WrapperTypeInfo::NodeClassId) {
            DCHECK(V8Node::hasInstance(wrapper, m_isolate));
            Node* node = V8Node::toImpl(wrapper);
            if (node->hasEventListeners())
                addReferencesForNodeWithEventListeners(m_isolate, node, v8::Persistent<v8::Object>::Cast(*value));
            Node* root = V8GCController::opaqueRootForGC(m_isolate, node);
            m_isolate->SetObjectGroupId(*value, v8::UniqueId(reinterpret_cast<intptr_t>(root)));
            if (m_constructRetainedObjectInfos)
                m_groupsWhichNeedRetainerInfo.append(root);
        } else if (classId == WrapperTypeInfo::ObjectClassId) {
            type->visitDOMWrapper(m_isolate, toScriptWrappable(wrapper), v8::Persistent<v8::Object>::Cast(*value));
        } else {
            ASSERT_NOT_REACHED();
        }
    }

    void notifyFinished()
    {
        if (!m_constructRetainedObjectInfos)
            return;
        std::sort(m_groupsWhichNeedRetainerInfo.begin(), m_groupsWhichNeedRetainerInfo.end());
        Node* alreadyAdded = 0;
        v8::HeapProfiler* profiler = m_isolate->GetHeapProfiler();
        for (size_t i = 0; i < m_groupsWhichNeedRetainerInfo.size(); ++i) {
            Node* root = m_groupsWhichNeedRetainerInfo[i];
            if (root != alreadyAdded) {
                profiler->SetRetainedObjectInfo(v8::UniqueId(reinterpret_cast<intptr_t>(root)), new RetainedDOMInfo(root));
                alreadyAdded = root;
            }
        }
        if (m_liveRootGroupIdSet)
            profiler->SetRetainedObjectInfo(liveRootId(), new ActiveDOMObjectsInfo(m_domObjectsWithPendingActivity));
    }

private:
    v8::UniqueId liveRootId()
    {
        const v8::Persistent<v8::Value>& liveRoot = V8PerIsolateData::from(m_isolate)->ensureLiveRoot();
        const intptr_t* idPointer = reinterpret_cast<const intptr_t*>(&liveRoot);
        v8::UniqueId id(*idPointer);
        if (!m_liveRootGroupIdSet) {
            m_isolate->SetObjectGroupId(liveRoot, id);
            m_liveRootGroupIdSet = true;
            ++m_domObjectsWithPendingActivity;
        }
        return id;
    }

    v8::Isolate* m_isolate;
    // v8 guarantees that Blink will not regain control while a v8 GC runs
    // (=> no Oilpan GCs will be triggered), hence raw, untraced members
    // can safely be kept here.
    Vector<UntracedMember<Node>> m_groupsWhichNeedRetainerInfo;
    int m_domObjectsWithPendingActivity;
    bool m_liveRootGroupIdSet;
    bool m_constructRetainedObjectInfos;
};

static unsigned long long usedHeapSize(v8::Isolate* isolate)
{
    v8::HeapStatistics heapStatistics;
    isolate->GetHeapStatistics(&heapStatistics);
    return heapStatistics.used_heap_size();
}

namespace {

void visitWeakHandlesForMinorGC(v8::Isolate* isolate)
{
    MinorGCUnmodifiedWrapperVisitor visitor(isolate);
    isolate->VisitWeakHandles(&visitor);
}

void objectGroupingForMajorGC(v8::Isolate* isolate, bool constructRetainedObjectInfos)
{
    MajorGCWrapperVisitor visitor(isolate, constructRetainedObjectInfos);
    isolate->VisitHandlesWithClassIds(&visitor);
    visitor.notifyFinished();
}

void gcPrologueForMajorGC(v8::Isolate* isolate, bool constructRetainedObjectInfos)
{
    // TODO(hlopko): Collect retained object infos for heap profiler
    if (!RuntimeEnabledFeatures::traceWrappablesEnabled()) {
        objectGroupingForMajorGC(isolate, constructRetainedObjectInfos);
    }
}

} // namespace

void V8GCController::gcPrologue(v8::Isolate* isolate, v8::GCType type, v8::GCCallbackFlags flags)
{
    if (isMainThread())
        ScriptForbiddenScope::enter();

    // Attribute garbage collection to the all frames instead of a specific
    // frame.
    if (BlameContext* blameContext = Platform::current()->topLevelBlameContext())
        blameContext->Enter();

    // TODO(haraken): A GC callback is not allowed to re-enter V8. This means
    // that it's unsafe to run Oilpan's GC in the GC callback because it may
    // run finalizers that call into V8. To avoid the risk, we should post
    // a task to schedule the Oilpan's GC.
    // (In practice, there is no finalizer that calls into V8 and thus is safe.)

    v8::HandleScope scope(isolate);
    switch (type) {
    case v8::kGCTypeScavenge:
        if (ThreadState::current())
            ThreadState::current()->willStartV8GC(BlinkGC::V8MinorGC);

        TRACE_EVENT_BEGIN1("devtools.timeline,v8", "MinorGC", "usedHeapSizeBefore", usedHeapSize(isolate));
        visitWeakHandlesForMinorGC(isolate);
        break;
    case v8::kGCTypeMarkSweepCompact:
        if (ThreadState::current())
            ThreadState::current()->willStartV8GC(BlinkGC::V8MajorGC);

        TRACE_EVENT_BEGIN2("devtools.timeline,v8", "MajorGC", "usedHeapSizeBefore", usedHeapSize(isolate), "type", "atomic pause");
        gcPrologueForMajorGC(isolate, flags & v8::kGCCallbackFlagConstructRetainedObjectInfos);
        break;
    case v8::kGCTypeIncrementalMarking:
        if (ThreadState::current())
            ThreadState::current()->willStartV8GC(BlinkGC::V8MajorGC);

        TRACE_EVENT_BEGIN2("devtools.timeline,v8", "MajorGC", "usedHeapSizeBefore", usedHeapSize(isolate), "type", "incremental marking");
        gcPrologueForMajorGC(isolate, flags & v8::kGCCallbackFlagConstructRetainedObjectInfos);
        break;
    case v8::kGCTypeProcessWeakCallbacks:
        TRACE_EVENT_BEGIN2("devtools.timeline,v8", "MajorGC", "usedHeapSizeBefore", usedHeapSize(isolate), "type", "weak processing");
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

namespace {

void UpdateCollectedPhantomHandles(v8::Isolate* isolate)
{
    ThreadHeapStats& heapStats = ThreadState::current()->heap().heapStats();
    size_t count = isolate->NumberOfPhantomHandleResetsSinceLastCall();
    heapStats.decreaseWrapperCount(count);
    heapStats.increaseCollectedWrapperCount(count);
}

} // namespace

void V8GCController::gcEpilogue(v8::Isolate* isolate, v8::GCType type, v8::GCCallbackFlags flags)
{
    UpdateCollectedPhantomHandles(isolate);
    switch (type) {
    case v8::kGCTypeScavenge:
        TRACE_EVENT_END1("devtools.timeline,v8", "MinorGC", "usedHeapSizeAfter", usedHeapSize(isolate));
        // TODO(haraken): Remove this. See the comment in gcPrologue.
        if (ThreadState::current())
            ThreadState::current()->scheduleV8FollowupGCIfNeeded(BlinkGC::V8MinorGC);
        break;
    case v8::kGCTypeMarkSweepCompact:
        TRACE_EVENT_END1("devtools.timeline,v8", "MajorGC", "usedHeapSizeAfter", usedHeapSize(isolate));
        if (ThreadState::current())
            ThreadState::current()->scheduleV8FollowupGCIfNeeded(BlinkGC::V8MajorGC);
        break;
    case v8::kGCTypeIncrementalMarking:
        TRACE_EVENT_END1("devtools.timeline,v8", "MajorGC", "usedHeapSizeAfter", usedHeapSize(isolate));
        break;
    case v8::kGCTypeProcessWeakCallbacks:
        TRACE_EVENT_END1("devtools.timeline,v8", "MajorGC", "usedHeapSizeAfter", usedHeapSize(isolate));
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    if (isMainThread())
        ScriptForbiddenScope::exit();

    if (BlameContext* blameContext = Platform::current()->topLevelBlameContext())
        blameContext->Leave();

    if (ThreadState::current() && !ThreadState::current()->isGCForbidden()) {
        // v8::kGCCallbackFlagForced forces a Blink heap garbage collection
        // when a garbage collection was forced from V8. This is either used
        // for tests that force GCs from JavaScript to verify that objects die
        // when expected.
        if (flags & v8::kGCCallbackFlagForced) {
            // This single GC is not enough for two reasons:
            //   (1) The GC is not precise because the GC scans on-stack pointers conservatively.
            //   (2) One GC is not enough to break a chain of persistent handles. It's possible that
            //       some heap allocated objects own objects that contain persistent handles
            //       pointing to other heap allocated objects. To break the chain, we need multiple GCs.
            //
            // Regarding (1), we force a precise GC at the end of the current event loop. So if you want
            // to collect all garbage, you need to wait until the next event loop.
            // Regarding (2), it would be OK in practice to trigger only one GC per gcEpilogue, because
            // GCController.collectAll() forces multiple V8's GC.
            ThreadHeap::collectGarbage(BlinkGC::HeapPointersOnStack, BlinkGC::GCWithSweep, BlinkGC::ForcedGC);

            // Forces a precise GC at the end of the current event loop.
            RELEASE_ASSERT(!ThreadState::current()->isInGC());
            ThreadState::current()->setGCState(ThreadState::FullGCScheduled);
        }

        // v8::kGCCallbackFlagCollectAllAvailableGarbage is used when V8 handles
        // low memory notifications.
        if (flags & v8::kGCCallbackFlagCollectAllAvailableGarbage) {
            // This single GC is not enough. See the above comment.
            ThreadHeap::collectGarbage(BlinkGC::HeapPointersOnStack, BlinkGC::GCWithSweep, BlinkGC::ForcedGC);

            // The conservative GC might have left floating garbage. Schedule
            // precise GC to ensure that we collect all available garbage.
            if (ThreadState::current())
                ThreadState::current()->schedulePreciseGC();
        }
    }

    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data", InspectorUpdateCountersEvent::data());
}

void V8GCController::collectGarbage(v8::Isolate* isolate)
{
    v8::HandleScope handleScope(isolate);
    RefPtr<ScriptState> scriptState = ScriptState::create(v8::Context::New(isolate), DOMWrapperWorld::create(isolate));
    ScriptState::Scope scope(scriptState.get());
    V8ScriptRunner::compileAndRunInternalScript(v8String(isolate, "if (gc) gc();"), isolate);
    scriptState->disposePerContextData();
}

void V8GCController::collectAllGarbageForTesting(v8::Isolate* isolate)
{
    for (unsigned i = 0; i < 5; i++)
        isolate->RequestGarbageCollectionForTesting(v8::Isolate::kFullGarbageCollection);
}

class DOMWrapperTracer : public v8::PersistentHandleVisitor {
public:
    explicit DOMWrapperTracer(Visitor* visitor)
        : m_visitor(visitor)
    {
    }

    void VisitPersistentHandle(v8::Persistent<v8::Value>* value, uint16_t classId) override
    {
        if (classId != WrapperTypeInfo::NodeClassId && classId != WrapperTypeInfo::ObjectClassId)
            return;

        const v8::Persistent<v8::Object>& wrapper = v8::Persistent<v8::Object>::Cast(*value);

        if (m_visitor)
            toWrapperTypeInfo(wrapper)->trace(m_visitor, toScriptWrappable(wrapper));
    }

private:
    Visitor* m_visitor;
};

void V8GCController::traceDOMWrappers(v8::Isolate* isolate, Visitor* visitor)
{
    DOMWrapperTracer tracer(visitor);
    isolate->VisitHandlesWithClassIds(&tracer);
}

class PendingActivityVisitor : public v8::PersistentHandleVisitor {
public:
    PendingActivityVisitor(v8::Isolate* isolate, ExecutionContext* executionContext)
        : m_isolate(isolate)
        , m_executionContext(executionContext)
        , m_pendingActivityFound(false)
    {
    }

    void VisitPersistentHandle(v8::Persistent<v8::Value>* value, uint16_t classId) override
    {
        // If we have already found any wrapper that has a pending activity,
        // we don't need to check other wrappers.
        if (m_pendingActivityFound)
            return;

        if (classId != WrapperTypeInfo::NodeClassId && classId != WrapperTypeInfo::ObjectClassId)
            return;

        v8::Local<v8::Object> wrapper = v8::Local<v8::Object>::New(m_isolate, v8::Persistent<v8::Object>::Cast(*value));
        ASSERT(V8DOMWrapper::hasInternalFieldsSet(wrapper));
        // The ExecutionContext check is heavy, so it should be done at the last.
        if (toWrapperTypeInfo(wrapper)->hasPendingActivity(wrapper)
            // TODO(haraken): Currently we don't have a way to get a creation
            // context from a wrapper. We should implement the way and enable
            // the following condition.
            //
            // This condition affects only compositor workers, where one isolate
            // is shared by multiple workers. If we don't have the condition,
            // a worker object for a compositor worker doesn't get collected
            // until all compositor workers in the same isolate lose pending
            // activities. In other words, not having the condition delays
            // destruction of a worker object of a compositor worker.
            //
            /* && toExecutionContext(wrapper->creationContext()) == m_executionContext */
            )
            m_pendingActivityFound = true;
    }

    bool pendingActivityFound() const { return m_pendingActivityFound; }

private:
    v8::Isolate* m_isolate;
    Persistent<ExecutionContext> m_executionContext;
    bool m_pendingActivityFound;
};

bool V8GCController::hasPendingActivity(v8::Isolate* isolate, ExecutionContext* executionContext)
{
    // V8GCController::hasPendingActivity is used only when a worker checks if
    // the worker contains any wrapper that has pending activities.
    ASSERT(!isMainThread());

    DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scanPendingActivityHistogram, new CustomCountHistogram("Blink.ScanPendingActivityDuration", 1, 1000, 50));
    double startTime = WTF::currentTimeMS();
    v8::HandleScope scope(isolate);
    PendingActivityVisitor visitor(isolate, executionContext);
    toIsolate(executionContext)->VisitHandlesWithClassIds(&visitor);
    scanPendingActivityHistogram.count(static_cast<int>(WTF::currentTimeMS() - startTime));
    return visitor.pendingActivityFound();
}

} // namespace blink
