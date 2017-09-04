// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/Performance.h"

#include "bindings/core/v8/PerformanceObserverCallback.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/timing/PerformanceBase.h"
#include "core/timing/PerformanceLongTaskTiming.h"
#include "core/timing/PerformanceObserver.h"
#include "core/timing/PerformanceObserverInit.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class TestPerformanceBase : public PerformanceBase {
 public:
  TestPerformanceBase() : PerformanceBase(0) {}
  ~TestPerformanceBase() {}

  ExecutionContext* getExecutionContext() const override { return nullptr; }

  int numActiveObservers() { return m_activeObservers.size(); }

  int numObservers() { return m_observers.size(); }

  bool hasPerformanceObserverFor(PerformanceEntry::EntryType entryType) {
    return hasObserverFor(entryType);
  }

  DEFINE_INLINE_TRACE() { PerformanceBase::trace(visitor); }
};

class PerformanceBaseTest : public ::testing::Test {
 protected:
  void initialize(ScriptState* scriptState) {
    v8::Local<v8::Function> callback =
        v8::Function::New(scriptState->context(), nullptr).ToLocalChecked();
    m_base = new TestPerformanceBase();
    m_cb = PerformanceObserverCallback::create(scriptState, callback);
    m_observer = PerformanceObserver::create(scriptState, m_base, m_cb);
  }

  int numPerformanceEntriesInObserver() {
    return m_observer->m_performanceEntries.size();
  }

  Persistent<TestPerformanceBase> m_base;
  Persistent<PerformanceObserver> m_observer;
  Persistent<PerformanceObserverCallback> m_cb;
};

TEST_F(PerformanceBaseTest, Register) {
  V8TestingScope scope;
  initialize(scope.getScriptState());

  EXPECT_EQ(0, m_base->numObservers());
  EXPECT_EQ(0, m_base->numActiveObservers());

  m_base->registerPerformanceObserver(*m_observer.get());
  EXPECT_EQ(1, m_base->numObservers());
  EXPECT_EQ(0, m_base->numActiveObservers());

  m_base->unregisterPerformanceObserver(*m_observer.get());
  EXPECT_EQ(0, m_base->numObservers());
  EXPECT_EQ(0, m_base->numActiveObservers());
}

TEST_F(PerformanceBaseTest, Activate) {
  V8TestingScope scope;
  initialize(scope.getScriptState());

  EXPECT_EQ(0, m_base->numObservers());
  EXPECT_EQ(0, m_base->numActiveObservers());

  m_base->registerPerformanceObserver(*m_observer.get());
  EXPECT_EQ(1, m_base->numObservers());
  EXPECT_EQ(0, m_base->numActiveObservers());

  m_base->activateObserver(*m_observer.get());
  EXPECT_EQ(1, m_base->numObservers());
  EXPECT_EQ(1, m_base->numActiveObservers());

  m_base->unregisterPerformanceObserver(*m_observer.get());
  EXPECT_EQ(0, m_base->numObservers());
  EXPECT_EQ(0, m_base->numActiveObservers());
}

TEST_F(PerformanceBaseTest, AddLongTaskTiming) {
  V8TestingScope scope;
  initialize(scope.getScriptState());

  // Add a long task entry, but no observer registered.
  m_base->addLongTaskTiming(1234, 5678, "www.foo.com/bar", nullptr);
  EXPECT_FALSE(m_base->hasPerformanceObserverFor(PerformanceEntry::LongTask));
  EXPECT_EQ(0, numPerformanceEntriesInObserver());  // has no effect

  // Make an observer for longtask
  NonThrowableExceptionState exceptionState;
  PerformanceObserverInit options;
  Vector<String> entryTypeVec;
  entryTypeVec.append("longtask");
  options.setEntryTypes(entryTypeVec);
  m_observer->observe(options, exceptionState);

  EXPECT_TRUE(m_base->hasPerformanceObserverFor(PerformanceEntry::LongTask));
  // Add a long task entry
  m_base->addLongTaskTiming(1234, 5678, "www.foo.com/bar", nullptr);
  EXPECT_EQ(1, numPerformanceEntriesInObserver());  // added an entry
}
}  // namespace blink
