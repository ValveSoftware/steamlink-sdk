// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptWrappableVisitor.h"

#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/TraceWrapperV8Reference.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/testing/DeathAwareScriptWrappable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

static void preciselyCollectGarbage() {
  ThreadState::current()->collectAllGarbage();
}

static void runV8Scavenger(v8::Isolate* isolate) {
  V8GCController::collectGarbage(isolate, true);
}

static void runV8FullGc(v8::Isolate* isolate) {
  V8GCController::collectGarbage(isolate, false);
}

static bool DequeContains(const WTF::Deque<WrapperMarkingData>& deque,
                          DeathAwareScriptWrappable* needle) {
  for (auto item : deque) {
    if (item.rawObjectPointer() == needle)
      return true;
  }
  return false;
}

TEST(ScriptWrappableVisitorTest, ScriptWrappableVisitorTracesWrappers) {
  V8TestingScope scope;
  if (!RuntimeEnabledFeatures::traceWrappablesEnabled()) {
    return;
  }
  ScriptWrappableVisitor* visitor =
      V8PerIsolateData::from(scope.isolate())->scriptWrappableVisitor();
  visitor->TracePrologue();

  DeathAwareScriptWrappable* target = DeathAwareScriptWrappable::create();
  DeathAwareScriptWrappable* dependency = DeathAwareScriptWrappable::create();
  target->setRawDependency(dependency);

  HeapObjectHeader* targetHeader = HeapObjectHeader::fromPayload(target);
  HeapObjectHeader* dependencyHeader =
      HeapObjectHeader::fromPayload(dependency);

  EXPECT_TRUE(visitor->getMarkingDeque()->isEmpty());
  EXPECT_FALSE(targetHeader->isWrapperHeaderMarked());
  EXPECT_FALSE(dependencyHeader->isWrapperHeaderMarked());

  std::pair<void*, void*> pair = std::make_pair(
      const_cast<WrapperTypeInfo*>(target->wrapperTypeInfo()), target);
  visitor->RegisterV8Reference(pair);
  EXPECT_EQ(visitor->getMarkingDeque()->size(), 1ul);

  visitor->AdvanceTracing(
      0, v8::EmbedderHeapTracer::AdvanceTracingActions(
             v8::EmbedderHeapTracer::ForceCompletionAction::FORCE_COMPLETION));
  v8::MicrotasksScope::PerformCheckpoint(scope.isolate());
  EXPECT_EQ(visitor->getMarkingDeque()->size(), 0ul);
  EXPECT_TRUE(targetHeader->isWrapperHeaderMarked());
  EXPECT_TRUE(dependencyHeader->isWrapperHeaderMarked());

  visitor->TraceEpilogue();
}

TEST(ScriptWrappableVisitorTest, OilpanCollectObjectsNotReachableFromV8) {
  V8TestingScope scope;
  if (!RuntimeEnabledFeatures::traceWrappablesEnabled()) {
    return;
  }
  v8::Isolate* isolate = scope.isolate();

  {
    v8::HandleScope handleScope(isolate);
    DeathAwareScriptWrappable* object = DeathAwareScriptWrappable::create();
    DeathAwareScriptWrappable::observeDeathsOf(object);

    // Creates new V8 wrapper and associates it with global scope
    toV8(object, scope.context()->Global(), isolate);
  }

  runV8Scavenger(isolate);
  runV8FullGc(isolate);
  preciselyCollectGarbage();

  EXPECT_TRUE(DeathAwareScriptWrappable::hasDied());
}

TEST(ScriptWrappableVisitorTest, OilpanDoesntCollectObjectsReachableFromV8) {
  V8TestingScope scope;
  if (!RuntimeEnabledFeatures::traceWrappablesEnabled()) {
    return;
  }
  v8::Isolate* isolate = scope.isolate();
  v8::HandleScope handleScope(isolate);
  DeathAwareScriptWrappable* object = DeathAwareScriptWrappable::create();
  DeathAwareScriptWrappable::observeDeathsOf(object);

  // Creates new V8 wrapper and associates it with global scope
  toV8(object, scope.context()->Global(), isolate);

  runV8Scavenger(isolate);
  runV8FullGc(isolate);
  preciselyCollectGarbage();

  EXPECT_FALSE(DeathAwareScriptWrappable::hasDied());
}

TEST(ScriptWrappableVisitorTest, V8ReportsLiveObjectsDuringScavenger) {
  V8TestingScope scope;
  if (!RuntimeEnabledFeatures::traceWrappablesEnabled()) {
    return;
  }
  v8::Isolate* isolate = scope.isolate();
  v8::HandleScope handleScope(isolate);
  DeathAwareScriptWrappable* object = DeathAwareScriptWrappable::create();
  DeathAwareScriptWrappable::observeDeathsOf(object);

  v8::Local<v8::Value> wrapper =
      toV8(object, scope.context()->Global(), isolate);
  EXPECT_TRUE(wrapper->IsObject());
  v8::Local<v8::Object> wrapperObject = wrapper->ToObject();
  // V8 collects wrappers with unmodified maps (as they can be recreated
  // without loosing any data if needed). We need to create some property on
  // wrapper so V8 will not see it as unmodified.
  EXPECT_TRUE(
      wrapperObject->CreateDataProperty(scope.context(), 1, wrapper).IsJust());

  runV8Scavenger(isolate);
  preciselyCollectGarbage();

  EXPECT_FALSE(DeathAwareScriptWrappable::hasDied());
}

TEST(ScriptWrappableVisitorTest, V8ReportsLiveObjectsDuringFullGc) {
  V8TestingScope scope;
  if (!RuntimeEnabledFeatures::traceWrappablesEnabled()) {
    return;
  }
  v8::Isolate* isolate = scope.isolate();
  v8::HandleScope handleScope(isolate);
  DeathAwareScriptWrappable* object = DeathAwareScriptWrappable::create();
  DeathAwareScriptWrappable::observeDeathsOf(object);

  toV8(object, scope.context()->Global(), isolate);

  runV8Scavenger(isolate);
  runV8FullGc(isolate);
  preciselyCollectGarbage();

  EXPECT_FALSE(DeathAwareScriptWrappable::hasDied());
}

TEST(ScriptWrappableVisitorTest, OilpanClearsHeadersWhenObjectDied) {
  V8TestingScope scope;
  if (!RuntimeEnabledFeatures::traceWrappablesEnabled()) {
    return;
  }

  DeathAwareScriptWrappable* object = DeathAwareScriptWrappable::create();
  ScriptWrappableVisitor* visitor =
      V8PerIsolateData::from(scope.isolate())->scriptWrappableVisitor();
  auto header = HeapObjectHeader::fromPayload(object);
  visitor->getHeadersToUnmark()->append(header);

  preciselyCollectGarbage();

  EXPECT_FALSE(visitor->getHeadersToUnmark()->contains(header));
  visitor->getHeadersToUnmark()->clear();
}

TEST(ScriptWrappableVisitorTest, OilpanClearsMarkingDequeWhenObjectDied) {
  V8TestingScope scope;
  if (!RuntimeEnabledFeatures::traceWrappablesEnabled()) {
    return;
  }

  DeathAwareScriptWrappable* object = DeathAwareScriptWrappable::create();
  ScriptWrappableVisitor* visitor =
      V8PerIsolateData::from(scope.isolate())->scriptWrappableVisitor();
  visitor->pushToMarkingDeque(
      TraceTrait<DeathAwareScriptWrappable>::markWrapper,
      TraceTrait<DeathAwareScriptWrappable>::heapObjectHeader, object);

  EXPECT_EQ(visitor->getMarkingDeque()->first().rawObjectPointer(), object);

  preciselyCollectGarbage();

  EXPECT_EQ(visitor->getMarkingDeque()->first().rawObjectPointer(), nullptr);

  visitor->getMarkingDeque()->clear();
  visitor->getVerifierDeque()->clear();
}

TEST(ScriptWrappableVisitorTest, NonMarkedObjectDoesNothingOnWriteBarrierHit) {
  V8TestingScope scope;
  if (!RuntimeEnabledFeatures::traceWrappablesEnabled()) {
    return;
  }

  ScriptWrappableVisitor* visitor =
      V8PerIsolateData::from(scope.isolate())->scriptWrappableVisitor();

  DeathAwareScriptWrappable* target = DeathAwareScriptWrappable::create();
  DeathAwareScriptWrappable* dependency = DeathAwareScriptWrappable::create();

  EXPECT_TRUE(visitor->getMarkingDeque()->isEmpty());

  target->setRawDependency(dependency);

  EXPECT_TRUE(visitor->getMarkingDeque()->isEmpty());
}

TEST(ScriptWrappableVisitorTest,
     MarkedObjectDoesNothingOnWriteBarrierHitWhenDependencyIsMarkedToo) {
  V8TestingScope scope;
  if (!RuntimeEnabledFeatures::traceWrappablesEnabled()) {
    return;
  }

  ScriptWrappableVisitor* visitor =
      V8PerIsolateData::from(scope.isolate())->scriptWrappableVisitor();

  DeathAwareScriptWrappable* target = DeathAwareScriptWrappable::create();
  DeathAwareScriptWrappable* dependencies[] = {
      DeathAwareScriptWrappable::create(), DeathAwareScriptWrappable::create(),
      DeathAwareScriptWrappable::create(), DeathAwareScriptWrappable::create(),
      DeathAwareScriptWrappable::create()};

  HeapObjectHeader::fromPayload(target)->markWrapperHeader();
  for (int i = 0; i < 5; i++) {
    HeapObjectHeader::fromPayload(dependencies[i])->markWrapperHeader();
  }

  EXPECT_TRUE(visitor->getMarkingDeque()->isEmpty());

  target->setRawDependency(dependencies[0]);
  target->setWrappedDependency(dependencies[1]);
  target->addWrappedVectorDependency(dependencies[2]);
  target->addWrappedHashMapDependency(dependencies[3], dependencies[4]);

  EXPECT_TRUE(visitor->getMarkingDeque()->isEmpty());
}

TEST(ScriptWrappableVisitorTest,
     MarkedObjectMarksDependencyOnWriteBarrierHitWhenNotMarked) {
  V8TestingScope scope;
  if (!RuntimeEnabledFeatures::traceWrappablesEnabled()) {
    return;
  }

  ScriptWrappableVisitor* visitor =
      V8PerIsolateData::from(scope.isolate())->scriptWrappableVisitor();

  DeathAwareScriptWrappable* target = DeathAwareScriptWrappable::create();
  DeathAwareScriptWrappable* dependencies[] = {
      DeathAwareScriptWrappable::create(), DeathAwareScriptWrappable::create(),
      DeathAwareScriptWrappable::create(), DeathAwareScriptWrappable::create(),
      DeathAwareScriptWrappable::create()};

  HeapObjectHeader::fromPayload(target)->markWrapperHeader();

  EXPECT_TRUE(visitor->getMarkingDeque()->isEmpty());

  target->setRawDependency(dependencies[0]);
  target->setWrappedDependency(dependencies[1]);
  target->addWrappedVectorDependency(dependencies[2]);
  target->addWrappedHashMapDependency(dependencies[3], dependencies[4]);

  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(DequeContains(*visitor->getMarkingDeque(), dependencies[i]));
  }

  visitor->getMarkingDeque()->clear();
  visitor->getVerifierDeque()->clear();
}

namespace {

class HandleContainer
    : public blink::GarbageCollectedFinalized<HandleContainer>,
      blink::TraceWrapperBase {
 public:
  static HandleContainer* create() { return new HandleContainer(); }
  virtual ~HandleContainer() {}

  DEFINE_INLINE_TRACE() {}
  DEFINE_INLINE_TRACE_WRAPPERS() {
    visitor->traceWrappers(m_handle.cast<v8::Value>());
  }

  void setValue(v8::Isolate* isolate, v8::Local<v8::String> string) {
    m_handle.set(isolate, string);
  }

 private:
  HandleContainer() : m_handle(this) {}

  TraceWrapperV8Reference<v8::String> m_handle;
};

class InterceptingScriptWrappableVisitor
    : public blink::ScriptWrappableVisitor {
 public:
  InterceptingScriptWrappableVisitor(v8::Isolate* isolate)
      : ScriptWrappableVisitor(isolate), m_markedWrappers(new size_t(0)) {}
  ~InterceptingScriptWrappableVisitor() { delete m_markedWrappers; }

  virtual void markWrapper(const v8::PersistentBase<v8::Value>* handle) const {
    *m_markedWrappers += 1;
    ScriptWrappableVisitor::markWrapper(handle);
  }

  size_t numberOfMarkedWrappers() const { return *m_markedWrappers; }

 private:
  size_t* m_markedWrappers;  // Indirection required because of const override.
};

void swapInNewVisitor(v8::Isolate* isolate,
                      blink::ScriptWrappableVisitor* newVisitor) {
  isolate->SetEmbedderHeapTracer(newVisitor);
  std::unique_ptr<blink::ScriptWrappableVisitor> visitor(newVisitor);
  V8PerIsolateData::from(isolate)->setScriptWrappableVisitor(
      std::move(visitor));
}

}  // namespace

TEST(ScriptWrappableVisitorTest, NoWriteBarrierOnUnmarkedContainer) {
  if (!RuntimeEnabledFeatures::traceWrappablesEnabled())
    return;
  V8TestingScope scope;
  auto rawVisitor = new InterceptingScriptWrappableVisitor(scope.isolate());
  swapInNewVisitor(scope.isolate(), rawVisitor);
  rawVisitor->TracePrologue();
  v8::Local<v8::String> str =
      v8::String::NewFromUtf8(scope.isolate(), "teststring",
                              v8::NewStringType::kNormal, sizeof("teststring"))
          .ToLocalChecked();
  HandleContainer* container = HandleContainer::create();
  CHECK_EQ(0u, rawVisitor->numberOfMarkedWrappers());
  container->setValue(scope.isolate(), str);
  CHECK_EQ(0u, rawVisitor->numberOfMarkedWrappers());
  // Gracefully terminate tracing.
  rawVisitor->AdvanceTracing(
      0, v8::EmbedderHeapTracer::AdvanceTracingActions(
             v8::EmbedderHeapTracer::ForceCompletionAction::FORCE_COMPLETION));
  rawVisitor->TraceEpilogue();
}

TEST(ScriptWrappableVisitorTest, WriteBarrierTriggersOnMarkedContainer) {
  if (!RuntimeEnabledFeatures::traceWrappablesEnabled())
    return;
  V8TestingScope scope;
  auto rawVisitor = new InterceptingScriptWrappableVisitor(scope.isolate());
  swapInNewVisitor(scope.isolate(), rawVisitor);
  rawVisitor->TracePrologue();
  v8::Local<v8::String> str =
      v8::String::NewFromUtf8(scope.isolate(), "teststring",
                              v8::NewStringType::kNormal, sizeof("teststring"))
          .ToLocalChecked();
  HandleContainer* container = HandleContainer::create();
  HeapObjectHeader::fromPayload(container)->markWrapperHeader();
  CHECK_EQ(0u, rawVisitor->numberOfMarkedWrappers());
  container->setValue(scope.isolate(), str);
  CHECK_EQ(1u, rawVisitor->numberOfMarkedWrappers());
  // Gracefully terminate tracing.
  rawVisitor->AdvanceTracing(
      0, v8::EmbedderHeapTracer::AdvanceTracingActions(
             v8::EmbedderHeapTracer::ForceCompletionAction::FORCE_COMPLETION));
  rawVisitor->TraceEpilogue();
}

TEST(ScriptWrappableVisitorTest, VtableAtObjectStart) {
  // This test makes sure that the subobject v8::EmbedderHeapTracer is placed
  // at the start of a ScriptWrappableVisitor object. We do this to mitigate
  // potential problems that could be caused by LTO when passing
  // v8::EmbedderHeapTracer across the API boundary.
  V8TestingScope scope;
  std::unique_ptr<blink::ScriptWrappableVisitor> visitor(
      new ScriptWrappableVisitor(scope.isolate()));
  CHECK_EQ(
      static_cast<void*>(visitor.get()),
      static_cast<void*>(dynamic_cast<v8::EmbedderHeapTracer*>(visitor.get())));
}

}  // namespace blink
