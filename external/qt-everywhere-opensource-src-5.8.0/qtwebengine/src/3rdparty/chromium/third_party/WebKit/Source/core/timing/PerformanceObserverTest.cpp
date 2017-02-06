// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformanceObserver.h"

#include "core/timing/Performance.h"
#include "core/timing/PerformanceBase.h"
#include "core/timing/PerformanceMark.h"
#include "core/timing/PerformanceObserverCallback.h"
#include "core/timing/PerformanceObserverInit.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MockPerformanceObserverCallback : public PerformanceObserverCallback {
public:
    MockPerformanceObserverCallback() {}
    ~MockPerformanceObserverCallback() override {}

    void handleEvent(PerformanceObserverEntryList*, PerformanceObserver*) override {}
    ExecutionContext* getExecutionContext() const override { return nullptr; }
};

class MockPerformanceBase : public PerformanceBase {
public:
    MockPerformanceBase()
        : PerformanceBase(0)
    {
    }
    ~MockPerformanceBase() {}

    ExecutionContext* getExecutionContext() const override { return nullptr; }
};

class PerformanceObserverTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_base = new MockPerformanceBase();
        m_cb = new MockPerformanceObserverCallback();
        m_observer = PerformanceObserver::create(m_base, m_cb);
    }

    bool isRegistered()
    {
        return m_observer->m_isRegistered;
    }
    int numPerformanceEntries()
    {
        return m_observer->m_performanceEntries.size();
    }
    void deliver()
    {
        m_observer->deliver();
    }

    Persistent<MockPerformanceBase> m_base;
    Persistent<MockPerformanceObserverCallback> m_cb;
    Persistent<PerformanceObserver> m_observer;
};

TEST_F(PerformanceObserverTest, Observe)
{
    NonThrowableExceptionState exceptionState;
    PerformanceObserverInit options;
    Vector<String> entryTypeVec;
    entryTypeVec.append("mark");
    options.setEntryTypes(entryTypeVec);

    m_observer->observe(options, exceptionState);
    EXPECT_TRUE(isRegistered());
}

TEST_F(PerformanceObserverTest, Enqueue)
{
    Persistent<PerformanceEntry> entry = PerformanceMark::create("m", 1234);
    EXPECT_EQ(0, numPerformanceEntries());

    m_observer->enqueuePerformanceEntry(*entry);
    EXPECT_EQ(1, numPerformanceEntries());
}

TEST_F(PerformanceObserverTest, Deliver)
{
    Persistent<PerformanceEntry> entry = PerformanceMark::create("m", 1234);
    EXPECT_EQ(0, numPerformanceEntries());

    m_observer->enqueuePerformanceEntry(*entry);
    EXPECT_EQ(1, numPerformanceEntries());

    deliver();
    EXPECT_EQ(0, numPerformanceEntries());
}

TEST_F(PerformanceObserverTest, Disconnect)
{
    Persistent<PerformanceEntry> entry = PerformanceMark::create("m", 1234);
    EXPECT_EQ(0, numPerformanceEntries());

    m_observer->enqueuePerformanceEntry(*entry);
    EXPECT_EQ(1, numPerformanceEntries());

    m_observer->disconnect();
    EXPECT_FALSE(isRegistered());
    EXPECT_EQ(0, numPerformanceEntries());
}
}
