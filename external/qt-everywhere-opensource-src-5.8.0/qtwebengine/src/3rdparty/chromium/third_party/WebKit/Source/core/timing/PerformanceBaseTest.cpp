// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/Performance.h"

#include "core/timing/PerformanceBase.h"
#include "core/timing/PerformanceObserver.h"
#include "core/timing/PerformanceObserverCallback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class TestPerformanceBase : public PerformanceBase {
public:
    TestPerformanceBase() : PerformanceBase(0) {}
    ~TestPerformanceBase() {}

    ExecutionContext* getExecutionContext() const override { return nullptr; }

    int numActiveObservers() { return m_activeObservers.size(); }

    int numObservers() { return m_observers.size(); }

    DEFINE_INLINE_TRACE()
    {
        PerformanceBase::trace(visitor);
    }
};

class MockPerformanceObserverCallback : public PerformanceObserverCallback {
public:
    MockPerformanceObserverCallback() {}
    ~MockPerformanceObserverCallback() {}

    void handleEvent(PerformanceObserverEntryList*, PerformanceObserver*) override {}
    ExecutionContext* getExecutionContext() const override { return nullptr; }

    DEFINE_INLINE_TRACE()
    {
        PerformanceObserverCallback::trace(visitor);
    }
};

class PerformanceBaseTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_base = new TestPerformanceBase();
        m_cb = new MockPerformanceObserverCallback();
        m_observer = PerformanceObserver::create(m_base, m_cb);
    }

    Persistent<TestPerformanceBase> m_base;
    Persistent<PerformanceObserver> m_observer;
    Persistent<MockPerformanceObserverCallback> m_cb;
};

TEST_F(PerformanceBaseTest, Register)
{
    EXPECT_EQ(0, m_base->numObservers());
    EXPECT_EQ(0, m_base->numActiveObservers());

    m_base->registerPerformanceObserver(*m_observer.get());
    EXPECT_EQ(1, m_base->numObservers());
    EXPECT_EQ(0, m_base->numActiveObservers());

    m_base->unregisterPerformanceObserver(*m_observer.get());
    EXPECT_EQ(0, m_base->numObservers());
    EXPECT_EQ(0, m_base->numActiveObservers());
}

TEST_F(PerformanceBaseTest, Activate)
{
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

} // namespace blink
