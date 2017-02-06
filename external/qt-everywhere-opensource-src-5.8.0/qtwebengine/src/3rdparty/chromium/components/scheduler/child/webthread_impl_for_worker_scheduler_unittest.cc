// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/webthread_impl_for_worker_scheduler.h"

#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "components/scheduler/child/web_scheduler_impl.h"
#include "components/scheduler/child/worker_scheduler_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"

using testing::_;
using testing::AnyOf;
using testing::ElementsAre;
using testing::Invoke;

namespace scheduler {
namespace {

class NopTask : public blink::WebTaskRunner::Task {
 public:
  ~NopTask() override {}

  void run() override {}
};

class MockTask : public blink::WebTaskRunner::Task {
 public:
  ~MockTask() override {}

  MOCK_METHOD0(run, void());
};

class MockIdleTask : public blink::WebThread::IdleTask {
 public:
  ~MockIdleTask() override {}

  MOCK_METHOD1(run, void(double deadline));
};

class TestObserver : public blink::WebThread::TaskObserver {
 public:
  explicit TestObserver(std::string* calls) : calls_(calls) {}

  ~TestObserver() override {}

  void willProcessTask() override { calls_->append(" willProcessTask"); }

  void didProcessTask() override { calls_->append(" didProcessTask"); }

 private:
  std::string* calls_;  // NOT OWNED
};

class TestTask : public blink::WebTaskRunner::Task {
 public:
  explicit TestTask(std::string* calls) : calls_(calls) {}

  ~TestTask() override {}

  void run() override { calls_->append(" run"); }

 private:
  std::string* calls_;  // NOT OWNED
};

void addTaskObserver(WebThreadImplForWorkerScheduler* thread,
                     TestObserver* observer) {
  thread->addTaskObserver(observer);
}

void removeTaskObserver(WebThreadImplForWorkerScheduler* thread,
                        TestObserver* observer) {
  thread->removeTaskObserver(observer);
}

void shutdownOnThread(WebThreadImplForWorkerScheduler* thread) {
  WebSchedulerImpl* web_scheduler_impl =
      static_cast<WebSchedulerImpl*>(thread->scheduler());
  web_scheduler_impl->shutdown();
}

}  // namespace

class WebThreadImplForWorkerSchedulerTest : public testing::Test {
 public:
  WebThreadImplForWorkerSchedulerTest() {}

  ~WebThreadImplForWorkerSchedulerTest() override {}

  void SetUp() override {
    thread_.reset(new WebThreadImplForWorkerScheduler("test thread"));
    thread_->Init();
  }

  void RunOnWorkerThread(const tracked_objects::Location& from_here,
                         const base::Closure& task) {
    base::WaitableEvent completion(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    thread_->GetTaskRunner()->PostTask(
        from_here,
        base::Bind(&WebThreadImplForWorkerSchedulerTest::RunOnWorkerThreadTask,
                   base::Unretained(this), task, &completion));
    completion.Wait();
  }

 protected:
  void RunOnWorkerThreadTask(const base::Closure& task,
                             base::WaitableEvent* completion) {
    task.Run();
    completion->Signal();
  }

  std::unique_ptr<WebThreadImplForWorkerScheduler> thread_;

  DISALLOW_COPY_AND_ASSIGN(WebThreadImplForWorkerSchedulerTest);
};

TEST_F(WebThreadImplForWorkerSchedulerTest, TestDefaultTask) {
  std::unique_ptr<MockTask> task(new MockTask());
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  EXPECT_CALL(*task, run());
  ON_CALL(*task, run())
      .WillByDefault(Invoke([&completion]() { completion.Signal(); }));

  thread_->getWebTaskRunner()->postTask(blink::WebTraceLocation(),
                                        task.release());
  completion.Wait();
}

TEST_F(WebThreadImplForWorkerSchedulerTest,
       TestTaskExecutedBeforeThreadDeletion) {
  std::unique_ptr<MockTask> task(new MockTask());
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  EXPECT_CALL(*task, run());
  ON_CALL(*task, run())
      .WillByDefault(Invoke([&completion]() { completion.Signal(); }));

  thread_->getWebTaskRunner()->postTask(blink::WebTraceLocation(),
                                        task.release());
  thread_.reset();
}

TEST_F(WebThreadImplForWorkerSchedulerTest, TestIdleTask) {
  std::unique_ptr<MockIdleTask> task(new MockIdleTask());
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  EXPECT_CALL(*task, run(_));
  ON_CALL(*task, run(_))
      .WillByDefault(Invoke([&completion](double) { completion.Signal(); }));

  thread_->postIdleTask(blink::WebTraceLocation(), task.release());
  // We need to post a wakeup task or idle work will never happen.
  thread_->getWebTaskRunner()->postDelayedTask(blink::WebTraceLocation(),
                                               new NopTask(), 50ll);

  completion.Wait();
}

TEST_F(WebThreadImplForWorkerSchedulerTest, TestTaskObserver) {
  std::string calls;
  TestObserver observer(&calls);

  RunOnWorkerThread(FROM_HERE,
                    base::Bind(&addTaskObserver, thread_.get(), &observer));
  thread_->getWebTaskRunner()->postTask(blink::WebTraceLocation(),
                                        new TestTask(&calls));
  RunOnWorkerThread(FROM_HERE,
                    base::Bind(&removeTaskObserver, thread_.get(), &observer));

  // We need to be careful what we test here.  We want to make sure the
  // observers are un in the expected order before and after the task.
  // Sometimes we get an internal scheduler task running before or after
  // TestTask as well. This is not a bug, and we need to make sure the test
  // doesn't fail when that happens.
  EXPECT_THAT(calls, testing::HasSubstr("willProcessTask run didProcessTask"));
}

TEST_F(WebThreadImplForWorkerSchedulerTest, TestShutdown) {
  std::unique_ptr<MockTask> task(new MockTask());
  std::unique_ptr<MockTask> delayed_task(new MockTask());

  EXPECT_CALL(*task, run()).Times(0);
  EXPECT_CALL(*delayed_task, run()).Times(0);

  RunOnWorkerThread(FROM_HERE, base::Bind(&shutdownOnThread, thread_.get()));
  thread_->getWebTaskRunner()->postTask(blink::WebTraceLocation(),
                                        task.release());
  thread_->getWebTaskRunner()->postDelayedTask(blink::WebTraceLocation(),
                                               task.release(), 50ll);
  thread_.reset();
}

}  // namespace scheduler
