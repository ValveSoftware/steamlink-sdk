// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformanceObserver.h"

#include "bindings/core/v8/PerformanceObserverCallback.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/timing/Performance.h"
#include "core/timing/PerformanceBase.h"
#include "core/timing/PerformanceMark.h"
#include "core/timing/PerformanceObserverInit.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MockPerformanceBase : public PerformanceBase {
 public:
  MockPerformanceBase() : PerformanceBase(0) {}
  ~MockPerformanceBase() {}

  ExecutionContext* getExecutionContext() const override { return nullptr; }
};

class PerformanceObserverTest : public ::testing::Test {
 protected:
  void initialize(ScriptState* scriptState) {
    v8::Local<v8::Function> callback =
        v8::Function::New(scriptState->context(), nullptr).ToLocalChecked();
    m_base = new MockPerformanceBase();
    m_cb = PerformanceObserverCallback::create(scriptState, callback);
    m_observer = PerformanceObserver::create(scriptState, m_base, m_cb);
  }

  bool isRegistered() { return m_observer->m_isRegistered; }
  int numPerformanceEntries() {
    return m_observer->m_performanceEntries.size();
  }
  void deliver() { m_observer->deliver(); }

  Persistent<MockPerformanceBase> m_base;
  Persistent<PerformanceObserverCallback> m_cb;
  Persistent<PerformanceObserver> m_observer;
};

TEST_F(PerformanceObserverTest, Observe) {
  V8TestingScope scope;
  initialize(scope.getScriptState());

  NonThrowableExceptionState exceptionState;
  PerformanceObserverInit options;
  Vector<String> entryTypeVec;
  entryTypeVec.append("mark");
  options.setEntryTypes(entryTypeVec);

  m_observer->observe(options, exceptionState);
  EXPECT_TRUE(isRegistered());
}

TEST_F(PerformanceObserverTest, Enqueue) {
  V8TestingScope scope;
  initialize(scope.getScriptState());

  Persistent<PerformanceEntry> entry = PerformanceMark::create("m", 1234);
  EXPECT_EQ(0, numPerformanceEntries());

  m_observer->enqueuePerformanceEntry(*entry);
  EXPECT_EQ(1, numPerformanceEntries());
}

TEST_F(PerformanceObserverTest, Deliver) {
  V8TestingScope scope;
  initialize(scope.getScriptState());

  Persistent<PerformanceEntry> entry = PerformanceMark::create("m", 1234);
  EXPECT_EQ(0, numPerformanceEntries());

  m_observer->enqueuePerformanceEntry(*entry);
  EXPECT_EQ(1, numPerformanceEntries());

  deliver();
  EXPECT_EQ(0, numPerformanceEntries());
}

TEST_F(PerformanceObserverTest, Disconnect) {
  V8TestingScope scope;
  initialize(scope.getScriptState());

  Persistent<PerformanceEntry> entry = PerformanceMark::create("m", 1234);
  EXPECT_EQ(0, numPerformanceEntries());

  m_observer->enqueuePerformanceEntry(*entry);
  EXPECT_EQ(1, numPerformanceEntries());

  m_observer->disconnect();
  EXPECT_FALSE(isRegistered());
  EXPECT_EQ(0, numPerformanceEntries());
}
}
