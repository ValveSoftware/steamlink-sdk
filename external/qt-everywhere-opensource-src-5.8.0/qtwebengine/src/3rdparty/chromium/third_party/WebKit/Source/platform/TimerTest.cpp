// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/Timer.h"

#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebViewScheduler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefCounted.h"
#include <memory>
#include <queue>

using testing::ElementsAre;

namespace blink {
namespace {
double gCurrentTimeSecs = 0.0;

// This class exists because gcc doesn't know how to move an std::unique_ptr.
class RefCountedTaskContainer : public RefCounted<RefCountedTaskContainer> {
public:
    explicit RefCountedTaskContainer(WebTaskRunner::Task* task) : m_task(wrapUnique(task)) { }

    ~RefCountedTaskContainer() { }

    void run()
    {
        m_task->run();
    }

private:
    std::unique_ptr<WebTaskRunner::Task> m_task;
};

class DelayedTask {
public:
    DelayedTask(WebTaskRunner::Task* task, double delaySeconds)
        : m_task(adoptRef(new RefCountedTaskContainer(task)))
        , m_runTimeSeconds(gCurrentTimeSecs + delaySeconds)
        , m_delaySeconds(delaySeconds)
    {
    }

    bool operator<(const DelayedTask& other) const
    {
        return m_runTimeSeconds > other.m_runTimeSeconds;
    }

    void run() const
    {
        m_task->run();
    }

    double runTimeSeconds() const
    {
        return m_runTimeSeconds;
    }

    double delaySeconds() const
    {
        return m_delaySeconds;
    }

private:
    RefPtr<RefCountedTaskContainer> m_task;
    double m_runTimeSeconds;
    double m_delaySeconds;
};

class MockWebTaskRunner : public WebTaskRunner {
public:
    explicit MockWebTaskRunner(std::priority_queue<DelayedTask>* timerTasks) : m_timerTasks(timerTasks) { }
    ~MockWebTaskRunner() override { }

    void postTask(const WebTraceLocation&, Task* task) override
    {
        m_timerTasks->push(DelayedTask(task, 0));
    }

    void postDelayedTask(const WebTraceLocation&, Task* task, double delayMs) override
    {
        m_timerTasks->push(DelayedTask(task, delayMs * 0.001));
    }

    WebTaskRunner* clone() override
    {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    double virtualTimeSeconds() const override
    {
        ASSERT_NOT_REACHED();
        return 0.0;
    }

    double monotonicallyIncreasingVirtualTimeSeconds() const override
    {
        return gCurrentTimeSecs;
    }

    std::priority_queue<DelayedTask>* m_timerTasks; // NOT OWNED
};

class MockWebScheduler : public WebScheduler {
public:
    MockWebScheduler() : m_timerWebTaskRunner(&m_timerTasks) { }
    ~MockWebScheduler() override { }

    bool shouldYieldForHighPriorityWork() override
    {
        return false;
    }

    bool canExceedIdleDeadlineIfRequired() override
    {
        return false;
    }

    void postIdleTask(const WebTraceLocation&, WebThread::IdleTask*) override
    {
    }

    void postNonNestableIdleTask(const WebTraceLocation&, WebThread::IdleTask*) override
    {
    }

    void postIdleTaskAfterWakeup(const WebTraceLocation&, WebThread::IdleTask*) override
    {
    }

    WebTaskRunner* timerTaskRunner() override
    {
        return &m_timerWebTaskRunner;
    }

    WebTaskRunner* loadingTaskRunner() override
    {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    void runUntilIdle()
    {
        while (m_timerTasks.size()) {
            gCurrentTimeSecs = m_timerTasks.top().runTimeSeconds();
            m_timerTasks.top().run();
            m_timerTasks.pop();
        }
    }

    void runUntilIdleOrDeadlinePassed(double deadline)
    {
        while (m_timerTasks.size()) {
            if (m_timerTasks.top().runTimeSeconds() > deadline) {
                gCurrentTimeSecs = deadline;
                break;
            }
            gCurrentTimeSecs = m_timerTasks.top().runTimeSeconds();
            m_timerTasks.top().run();
            m_timerTasks.pop();
        }
    }

    void runPendingTasks()
    {
        while (m_timerTasks.size() && m_timerTasks.top().runTimeSeconds() <= gCurrentTimeSecs) {
            m_timerTasks.top().run();
            m_timerTasks.pop();
        }
    }

    bool hasOneTimerTask() const
    {
        return m_timerTasks.size() == 1;
    }

    double nextTimerTaskDelaySecs() const
    {
        ASSERT(hasOneTimerTask());
        return m_timerTasks.top().delaySeconds();
    }

    void shutdown() override {}
    std::unique_ptr<WebViewScheduler> createWebViewScheduler(blink::WebView*) override { return nullptr; }
    void suspendTimerQueue() override { }
    void resumeTimerQueue() override { }
    void addPendingNavigation(WebScheduler::NavigatingFrameType) override { }
    void removePendingNavigation(WebScheduler::NavigatingFrameType) override { }
    void onNavigationStarted() override { }

private:
    std::priority_queue<DelayedTask> m_timerTasks;
    MockWebTaskRunner m_timerWebTaskRunner;
};

class FakeWebThread : public WebThread {
public:
    FakeWebThread() : m_webScheduler(wrapUnique(new MockWebScheduler())) { }
    ~FakeWebThread() override { }

    virtual bool isCurrentThread() const
    {
        ASSERT_NOT_REACHED();
        return true;
    }

    virtual PlatformThreadId threadId() const
    {
        ASSERT_NOT_REACHED();
        return 0;
    }

    WebTaskRunner* getWebTaskRunner() override
    {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    WebScheduler* scheduler() const override
    {
        return m_webScheduler.get();
    }

    virtual void enterRunLoop()
    {
        ASSERT_NOT_REACHED();
    }

    virtual void exitRunLoop()
    {
        ASSERT_NOT_REACHED();
    }

private:
    std::unique_ptr<MockWebScheduler> m_webScheduler;
};

class TimerTestPlatform : public TestingPlatformSupport {
public:
    TimerTestPlatform()
        : m_webThread(wrapUnique(new FakeWebThread())) { }
    ~TimerTestPlatform() override { }

    WebThread* currentThread() override
    {
        return m_webThread.get();
    }

    void runUntilIdle()
    {
        mockScheduler()->runUntilIdle();
    }

    void runPendingTasks()
    {
        mockScheduler()->runPendingTasks();
    }

    void runUntilIdleOrDeadlinePassed(double deadline)
    {
        mockScheduler()->runUntilIdleOrDeadlinePassed(deadline);
    }

    bool hasOneTimerTask() const
    {
        return mockScheduler()->hasOneTimerTask();
    }

    double nextTimerTaskDelaySecs() const
    {
        return mockScheduler()->nextTimerTaskDelaySecs();
    }

private:
    MockWebScheduler* mockScheduler() const
    {
        return static_cast<MockWebScheduler*>(m_webThread->scheduler());
    }

    std::unique_ptr<FakeWebThread> m_webThread;
};

class TimerTest : public testing::Test {
public:
    void SetUp() override
    {
        m_runTimes.clear();
        gCurrentTimeSecs = 10.0;
        m_startTime = gCurrentTimeSecs;
    }

    void countingTask(Timer<TimerTest>*)
    {
        m_runTimes.append(gCurrentTimeSecs);
    }

    void recordNextFireTimeTask(Timer<TimerTest>* timer)
    {
        m_nextFireTimes.append(gCurrentTimeSecs + timer->nextFireInterval());
    }

    void advanceTimeBy(double timeSecs)
    {
        gCurrentTimeSecs += timeSecs;
    }

    void runUntilIdle()
    {
        m_platform.runUntilIdle();
    }

    void runPendingTasks()
    {
        m_platform.runPendingTasks();
    }

    void runUntilIdleOrDeadlinePassed(double deadline)
    {
        m_platform.runUntilIdleOrDeadlinePassed(deadline);
    }

    bool hasOneTimerTask() const
    {
        return m_platform.hasOneTimerTask();
    }

    double nextTimerTaskDelaySecs() const
    {
        return m_platform.nextTimerTaskDelaySecs();
    }

protected:
    double m_startTime;
    WTF::Vector<double> m_runTimes;
    WTF::Vector<double> m_nextFireTimes;

private:
    TimerTestPlatform m_platform;
};

TEST_F(TimerTest, StartOneShot_Zero)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(0, BLINK_FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_FLOAT_EQ(0.0, nextTimerTaskDelaySecs());

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime));
}

TEST_F(TimerTest, StartOneShot_ZeroAndCancel)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(0, BLINK_FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_FLOAT_EQ(0.0, nextTimerTaskDelaySecs());

    timer.stop();

    runUntilIdle();
    EXPECT_FALSE(m_runTimes.size());
}

TEST_F(TimerTest, StartOneShot_ZeroAndCancelThenRepost)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(0, BLINK_FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_FLOAT_EQ(0.0, nextTimerTaskDelaySecs());

    timer.stop();

    runUntilIdle();
    EXPECT_FALSE(m_runTimes.size());

    timer.startOneShot(0, BLINK_FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_FLOAT_EQ(0.0, nextTimerTaskDelaySecs());

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime));
}

TEST_F(TimerTest, StartOneShot_Zero_RepostingAfterRunning)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(0, BLINK_FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_FLOAT_EQ(0.0, nextTimerTaskDelaySecs());

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime));

    timer.startOneShot(0, BLINK_FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_FLOAT_EQ(0.0, nextTimerTaskDelaySecs());

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime, m_startTime));
}

TEST_F(TimerTest, StartOneShot_NonZero)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10.0, BLINK_FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_FLOAT_EQ(10.0, nextTimerTaskDelaySecs());

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 10.0));
}

TEST_F(TimerTest, StartOneShot_NonZeroAndCancel)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10, BLINK_FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_FLOAT_EQ(10.0, nextTimerTaskDelaySecs());

    timer.stop();

    runUntilIdle();
    EXPECT_FALSE(m_runTimes.size());
}

TEST_F(TimerTest, StartOneShot_NonZeroAndCancelThenRepost)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10, BLINK_FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_FLOAT_EQ(10.0, nextTimerTaskDelaySecs());

    timer.stop();

    runUntilIdle();
    EXPECT_FALSE(m_runTimes.size());

    double secondPostTime = gCurrentTimeSecs;
    timer.startOneShot(10, BLINK_FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_FLOAT_EQ(10.0, nextTimerTaskDelaySecs());

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(secondPostTime + 10.0));
}

TEST_F(TimerTest, StartOneShot_NonZero_RepostingAfterRunning)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10, BLINK_FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_FLOAT_EQ(10.0, nextTimerTaskDelaySecs());

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 10.0));

    timer.startOneShot(20, BLINK_FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_FLOAT_EQ(20.0, nextTimerTaskDelaySecs());

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 10.0, m_startTime + 30.0));
}

TEST_F(TimerTest, PostingTimerTwiceWithSameRunTimeDoesNothing)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10, BLINK_FROM_HERE);
    timer.startOneShot(10, BLINK_FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_FLOAT_EQ(10.0, nextTimerTaskDelaySecs());

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 10.0));
}

TEST_F(TimerTest, PostingTimerTwiceWithNewerRunTimeCancelsOriginalTask)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10, BLINK_FROM_HERE);
    timer.startOneShot(0, BLINK_FROM_HERE);

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 0.0));
}

TEST_F(TimerTest, PostingTimerTwiceWithLaterRunTimeCancelsOriginalTask)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(0, BLINK_FROM_HERE);
    timer.startOneShot(10, BLINK_FROM_HERE);

    runUntilIdle();
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 10.0));
}

TEST_F(TimerTest, StartRepeatingTask)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startRepeating(1.0, BLINK_FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_FLOAT_EQ(1.0, nextTimerTaskDelaySecs());

    runUntilIdleOrDeadlinePassed(m_startTime + 5.5);
    EXPECT_THAT(m_runTimes, ElementsAre(
        m_startTime + 1.0, m_startTime + 2.0, m_startTime + 3.0, m_startTime + 4.0, m_startTime + 5.0));
}

TEST_F(TimerTest, StartRepeatingTask_ThenCancel)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startRepeating(1.0, BLINK_FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_FLOAT_EQ(1.0, nextTimerTaskDelaySecs());

    runUntilIdleOrDeadlinePassed(m_startTime + 2.5);
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 1.0, m_startTime + 2.0));

    timer.stop();
    runUntilIdle();

    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 1.0, m_startTime + 2.0));
}

TEST_F(TimerTest, StartRepeatingTask_ThenPostOneShot)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startRepeating(1.0, BLINK_FROM_HERE);

    ASSERT(hasOneTimerTask());
    EXPECT_FLOAT_EQ(1.0, nextTimerTaskDelaySecs());

    runUntilIdleOrDeadlinePassed(m_startTime + 2.5);
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 1.0, m_startTime + 2.0));

    timer.startOneShot(0, BLINK_FROM_HERE);
    runUntilIdle();

    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 1.0, m_startTime + 2.0, m_startTime + 2.5));
}

TEST_F(TimerTest, IsActive_NeverPosted)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);

    EXPECT_FALSE(timer.isActive());
}

TEST_F(TimerTest, IsActive_AfterPosting_OneShotZero)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(0, BLINK_FROM_HERE);

    EXPECT_TRUE(timer.isActive());
}

TEST_F(TimerTest, IsActive_AfterPosting_OneShotNonZero)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10, BLINK_FROM_HERE);

    EXPECT_TRUE(timer.isActive());
}

TEST_F(TimerTest, IsActive_AfterPosting_Repeating)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startRepeating(1.0, BLINK_FROM_HERE);

    EXPECT_TRUE(timer.isActive());
}

TEST_F(TimerTest, IsActive_AfterRunning_OneShotZero)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(0, BLINK_FROM_HERE);

    runUntilIdle();
    EXPECT_FALSE(timer.isActive());
}

TEST_F(TimerTest, IsActive_AfterRunning_OneShotNonZero)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10, BLINK_FROM_HERE);

    runUntilIdle();
    EXPECT_FALSE(timer.isActive());
}

TEST_F(TimerTest, IsActive_AfterRunning_Repeating)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startRepeating(1.0, BLINK_FROM_HERE);

    runUntilIdleOrDeadlinePassed(m_startTime + 10);
    EXPECT_TRUE(timer.isActive()); // It should run until cancelled.
}

TEST_F(TimerTest, NextFireInterval_OneShotZero)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(0, BLINK_FROM_HERE);

    EXPECT_FLOAT_EQ(0.0, timer.nextFireInterval());
}

TEST_F(TimerTest, NextFireInterval_OneShotNonZero)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10, BLINK_FROM_HERE);

    EXPECT_FLOAT_EQ(10.0, timer.nextFireInterval());
}

TEST_F(TimerTest, NextFireInterval_OneShotNonZero_AfterAFewSeconds)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10, BLINK_FROM_HERE);

    advanceTimeBy(2.0);
    EXPECT_FLOAT_EQ(8.0, timer.nextFireInterval());
}

TEST_F(TimerTest, NextFireInterval_Repeating)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startRepeating(20, BLINK_FROM_HERE);

    EXPECT_FLOAT_EQ(20.0, timer.nextFireInterval());
}

TEST_F(TimerTest, RepeatInterval_NeverStarted)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);

    EXPECT_FLOAT_EQ(0.0, timer.repeatInterval());
}

TEST_F(TimerTest, RepeatInterval_OneShotZero)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(0, BLINK_FROM_HERE);

    EXPECT_FLOAT_EQ(0.0, timer.repeatInterval());
}

TEST_F(TimerTest, RepeatInterval_OneShotNonZero)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startOneShot(10, BLINK_FROM_HERE);

    EXPECT_FLOAT_EQ(0.0, timer.repeatInterval());
}

TEST_F(TimerTest, RepeatInterval_Repeating)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startRepeating(20, BLINK_FROM_HERE);

    EXPECT_FLOAT_EQ(20.0, timer.repeatInterval());
}

TEST_F(TimerTest, AugmentRepeatInterval)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startRepeating(10, BLINK_FROM_HERE);
    EXPECT_FLOAT_EQ(10.0, timer.repeatInterval());
    EXPECT_FLOAT_EQ(10.0, timer.nextFireInterval());

    advanceTimeBy(2.0);
    timer.augmentRepeatInterval(10);

    EXPECT_FLOAT_EQ(20.0, timer.repeatInterval());
    EXPECT_FLOAT_EQ(18.0, timer.nextFireInterval());

    runUntilIdleOrDeadlinePassed(m_startTime + 50.0);
    EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 20.0, m_startTime + 40.0));
}

TEST_F(TimerTest, AugmentRepeatInterval_TimerFireDelayed)
{
    Timer<TimerTest> timer(this, &TimerTest::countingTask);
    timer.startRepeating(10, BLINK_FROM_HERE);
    EXPECT_FLOAT_EQ(10.0, timer.repeatInterval());
    EXPECT_FLOAT_EQ(10.0, timer.nextFireInterval());

    advanceTimeBy(123.0); // Make the timer long overdue.
    timer.augmentRepeatInterval(10);

    EXPECT_FLOAT_EQ(20.0, timer.repeatInterval());
    // The timer is overdue so it should be scheduled to fire immediatly.
    EXPECT_FLOAT_EQ(0.0, timer.nextFireInterval());
}

TEST_F(TimerTest, RepeatingTimerDoesNotDrift)
{
    Timer<TimerTest> timer(this, &TimerTest::recordNextFireTimeTask);
    timer.startRepeating(2.0, BLINK_FROM_HERE);

    ASSERT(hasOneTimerTask());
    recordNextFireTimeTask(&timer); // Next scheduled task to run at m_startTime + 2.0

    // Simulate timer firing early. Next scheduled task to run at m_startTime + 4.0
    advanceTimeBy(1.9);
    runUntilIdleOrDeadlinePassed(gCurrentTimeSecs + 0.2);

    advanceTimeBy(2.0);
    runPendingTasks(); // Next scheduled task to run at m_startTime + 6.0

    advanceTimeBy(2.1);
    runPendingTasks(); // Next scheduled task to run at m_startTime + 8.0

    advanceTimeBy(2.9);
    runPendingTasks(); // Next scheduled task to run at m_startTime + 10.0

    advanceTimeBy(3.1);
    runPendingTasks(); // Next scheduled task to run at m_startTime + 14.0 (skips a beat)

    advanceTimeBy(4.0);
    runPendingTasks(); // Next scheduled task to run at m_startTime + 18.0 (skips a beat)

    advanceTimeBy(10.0); // Next scheduled task to run at m_startTime + 28.0 (skips 5 beats)
    runPendingTasks();

    runUntilIdleOrDeadlinePassed(m_startTime + 5.5);
    EXPECT_THAT(m_nextFireTimes, ElementsAre(
        m_startTime + 2.0,
        m_startTime + 4.0,
        m_startTime + 6.0,
        m_startTime + 8.0,
        m_startTime + 10.0,
        m_startTime + 14.0,
        m_startTime + 18.0,
        m_startTime + 28.0));
}

template <typename TimerFiredClass>
class TimerForTest : public Timer<TimerFiredClass> {
public:
    using TimerFiredFunction = void (TimerFiredClass::*)(Timer<TimerFiredClass>*);

    ~TimerForTest() override { }

    TimerForTest(TimerFiredClass* timerFiredClass, TimerFiredFunction timerFiredFunction, WebTaskRunner* webTaskRunner)
        : Timer<TimerFiredClass>(timerFiredClass, timerFiredFunction, webTaskRunner)
    {
    }
};

TEST_F(TimerTest, UserSuppliedWebTaskRunner)
{
    std::priority_queue<DelayedTask> timerTasks;
    MockWebTaskRunner taskRunner(&timerTasks);
    TimerForTest<TimerTest> timer(this, &TimerTest::countingTask, &taskRunner);
    timer.startOneShot(0, BLINK_FROM_HERE);

    // Make sure the task was posted on taskRunner.
    EXPECT_FALSE(timerTasks.empty());
}


} // namespace
} // namespace blink
