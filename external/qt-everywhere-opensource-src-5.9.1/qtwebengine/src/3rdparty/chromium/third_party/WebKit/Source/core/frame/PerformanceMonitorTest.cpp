// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/PerformanceMonitor.h"

#include "core/frame/LocalFrame.h"
#include "core/frame/Location.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"

#include <memory>

namespace blink {

class PerformanceMonitorTest : public ::testing::Test {
 protected:
  void SetUp() override;
  LocalFrame* frame() const { return m_pageHolder->document().frame(); }
  ExecutionContext* executionContext() const {
    return &m_pageHolder->document();
  }
  LocalFrame* anotherFrame() const {
    return m_anotherPageHolder->document().frame();
  }
  ExecutionContext* anotherExecutionContext() const {
    return &m_anotherPageHolder->document();
  }

  void willExecuteScript(ExecutionContext* executionContext) {
    m_monitor->innerWillExecuteScript(executionContext);
  }

  void willProcessTask() { m_monitor->willProcessTask(); }

  void didProcessTask() { m_monitor->didProcessTask(); }

  // scheduler::TaskTimeObserver implementation
  void ReportTaskTime(scheduler::TaskQueue* queue,
                      double startTime,
                      double endTime) {
    m_monitor->ReportTaskTime(queue, startTime, endTime);
  }

  String frameContextURL();
  int numUniqueFrameContextsSeen();

  Persistent<PerformanceMonitor> m_monitor;
  std::unique_ptr<DummyPageHolder> m_pageHolder;
  std::unique_ptr<DummyPageHolder> m_anotherPageHolder;
};

void PerformanceMonitorTest::SetUp() {
  m_pageHolder = DummyPageHolder::create(IntSize(800, 600));
  m_pageHolder->document().setURL(KURL(KURL(), "https://example.com/foo"));
  m_monitor = new PerformanceMonitor(frame());

  // Create another dummy page holder and pretend this is the iframe.
  m_anotherPageHolder = DummyPageHolder::create(IntSize(400, 300));
  m_anotherPageHolder->document().setURL(
      KURL(KURL(), "https://iframed.com/bar"));
}

String PerformanceMonitorTest::frameContextURL() {
  // This is reported only if there is a single frameContext URL.
  if (m_monitor->m_frameContexts.size() != 1)
    return "";
  Frame* frame = (*m_monitor->m_frameContexts.begin()).get();
  return toLocalFrame(frame)->document()->location()->href();
}

int PerformanceMonitorTest::numUniqueFrameContextsSeen() {
  return m_monitor->m_frameContexts.size();
}

TEST_F(PerformanceMonitorTest, SingleScriptInTask) {
  willProcessTask();
  EXPECT_EQ(0, numUniqueFrameContextsSeen());
  willExecuteScript(executionContext());
  EXPECT_EQ(1, numUniqueFrameContextsSeen());
  didProcessTask();
  ReportTaskTime(nullptr, 3719349.445172, 3719349.5561923);  // Long task
  EXPECT_EQ(1, numUniqueFrameContextsSeen());
  EXPECT_EQ("https://example.com/foo", frameContextURL());
}

TEST_F(PerformanceMonitorTest, MultipleScriptsInTask_SingleContext) {
  willProcessTask();
  EXPECT_EQ(0, numUniqueFrameContextsSeen());
  willExecuteScript(executionContext());
  EXPECT_EQ(1, numUniqueFrameContextsSeen());
  EXPECT_EQ("https://example.com/foo", frameContextURL());

  willExecuteScript(executionContext());
  EXPECT_EQ(1, numUniqueFrameContextsSeen());
  didProcessTask();
  ReportTaskTime(nullptr, 3719349.445172, 3719349.5561923);  // Long task
  EXPECT_EQ(1, numUniqueFrameContextsSeen());
  EXPECT_EQ("https://example.com/foo", frameContextURL());
}

TEST_F(PerformanceMonitorTest, MultipleScriptsInTask_MultipleContexts) {
  willProcessTask();
  EXPECT_EQ(0, numUniqueFrameContextsSeen());
  willExecuteScript(executionContext());
  EXPECT_EQ(1, numUniqueFrameContextsSeen());
  EXPECT_EQ("https://example.com/foo", frameContextURL());

  willExecuteScript(anotherExecutionContext());
  EXPECT_EQ(2, numUniqueFrameContextsSeen());
  didProcessTask();
  ReportTaskTime(nullptr, 3719349.445172, 3719349.5561923);  // Long task
  EXPECT_EQ(2, numUniqueFrameContextsSeen());
  EXPECT_EQ("", frameContextURL());
}

TEST_F(PerformanceMonitorTest, NoScriptInLongTask) {
  willProcessTask();
  willExecuteScript(executionContext());
  didProcessTask();
  ReportTaskTime(nullptr, 3719349.445172, 3719349.445182);
  willProcessTask();
  didProcessTask();
  ReportTaskTime(nullptr, 3719349.445172, 3719349.5561923);  // Long task
  // Without presence of Script, FrameContext URL is not available
  EXPECT_EQ(0, numUniqueFrameContextsSeen());
}

}  // namespace blink
