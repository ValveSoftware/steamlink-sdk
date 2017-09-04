/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef TestingPlatformSupport_h
#define TestingPlatformSupport_h

#include "platform/PlatformExport.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebThread.h"
#include "wtf/Vector.h"
#include <memory>

namespace base {
class SimpleTestTickClock;
class TestDiscardableMemoryAllocator;
}

namespace cc {
class OrderedSimpleTaskRunner;
}

namespace cc_blink {
class WebCompositorSupportImpl;
}  // namespace cc_blink

namespace blink {
namespace scheduler {
class RendererScheduler;
class RendererSchedulerImpl;
}
class TestingPlatformMockWebTaskRunner;
class WebCompositorSupport;
class WebThread;

class TestingCompositorSupport : public WebCompositorSupport {
  std::unique_ptr<WebLayer> createLayer() override;
  std::unique_ptr<WebLayer> createLayerFromCCLayer(cc::Layer*) override;
  std::unique_ptr<WebContentLayer> createContentLayer(
      WebContentLayerClient*) override;
  std::unique_ptr<WebExternalTextureLayer> createExternalTextureLayer(
      cc::TextureLayerClient*) override;
  std::unique_ptr<WebImageLayer> createImageLayer() override;
  std::unique_ptr<WebScrollbarLayer> createScrollbarLayer(
      std::unique_ptr<WebScrollbar>,
      WebScrollbarThemePainter,
      std::unique_ptr<WebScrollbarThemeGeometry>) override;
  std::unique_ptr<WebScrollbarLayer> createSolidColorScrollbarLayer(
      WebScrollbar::Orientation,
      int thumbThickness,
      int trackStart,
      bool isLeftSideVerticalScrollbar) override;
};

class TestingPlatformMockScheduler : public WebScheduler {
  WTF_MAKE_NONCOPYABLE(TestingPlatformMockScheduler);

 public:
  TestingPlatformMockScheduler();
  ~TestingPlatformMockScheduler() override;

  void runSingleTask();
  void runAllTasks();

  // WebScheduler implementation:
  WebTaskRunner* loadingTaskRunner() override;
  WebTaskRunner* timerTaskRunner() override;
  void shutdown() override {}
  bool shouldYieldForHighPriorityWork() override { return false; }
  bool canExceedIdleDeadlineIfRequired() override { return false; }
  void postIdleTask(const WebTraceLocation&, WebThread::IdleTask*) override {}
  void postNonNestableIdleTask(const WebTraceLocation&,
                               WebThread::IdleTask*) override {}
  std::unique_ptr<WebViewScheduler> createWebViewScheduler(
      InterventionReporter*,
      WebViewScheduler::WebViewSchedulerSettings*) override {
    return nullptr;
  }
  void suspendTimerQueue() override {}
  void resumeTimerQueue() override {}
  void addPendingNavigation(WebScheduler::NavigatingFrameType) override {}
  void removePendingNavigation(WebScheduler::NavigatingFrameType) override {}
  void onNavigationStarted() override {}

 private:
  WTF::Deque<std::unique_ptr<WebTaskRunner::Task>> m_tasks;
  std::unique_ptr<TestingPlatformMockWebTaskRunner> m_mockWebTaskRunner;
};

class TestingPlatformSupport : public Platform {
  WTF_MAKE_NONCOPYABLE(TestingPlatformSupport);

 public:
  struct Config {
    WebCompositorSupport* compositorSupport = nullptr;
  };

  TestingPlatformSupport();
  explicit TestingPlatformSupport(const Config&);

  ~TestingPlatformSupport() override;

  // Platform:
  WebString defaultLocale() override;
  WebCompositorSupport* compositorSupport() override;
  WebThread* currentThread() override;
  WebBlobRegistry* getBlobRegistry() override;
  WebClipboard* clipboard() override;
  WebFileUtilities* fileUtilities() override;
  WebIDBFactory* idbFactory() override;
  WebURLLoaderMockFactory* getURLLoaderMockFactory() override;
  blink::WebURLLoader* createURLLoader() override;
  WebData loadResource(const char* name) override;
  WebURLError cancelledError(const WebURL&) const override;
  InterfaceProvider* interfaceProvider() override;

 protected:
  class TestingInterfaceProvider;

  const Config m_config;
  Platform* const m_oldPlatform;
  std::unique_ptr<TestingInterfaceProvider> m_interfaceProvider;
};

class TestingPlatformSupportWithMockScheduler : public TestingPlatformSupport {
  WTF_MAKE_NONCOPYABLE(TestingPlatformSupportWithMockScheduler);

 public:
  TestingPlatformSupportWithMockScheduler();
  explicit TestingPlatformSupportWithMockScheduler(const Config&);
  ~TestingPlatformSupportWithMockScheduler() override;

  // Platform:
  WebThread* currentThread() override;

  // Runs a single task.
  void runSingleTask();

  // Runs all currently queued immediate tasks and delayed tasks whose delay has
  // expired plus any immediate tasks that are posted as a result of running
  // those tasks.
  //
  // This function ignores future delayed tasks when deciding if the system is
  // idle.  If you need to ensure delayed tasks run, try runForPeriodSeconds()
  // instead.
  void runUntilIdle();

  // Runs for |seconds| the testing clock is advanced by |seconds|.  Note real
  // time elapsed will typically much less than |seconds| because delays between
  // timers are fast forwarded.
  void runForPeriodSeconds(double seconds);

  // Advances |m_clock| by |seconds|.
  void advanceClockSeconds(double seconds);

  scheduler::RendererScheduler* rendererScheduler() const;

  // Controls the behavior of |m_mockTaskRunner| if true, then |m_clock| will
  // be advanced to the next timer when there's no more immediate work to do.
  void setAutoAdvanceNowToPendingTasks(bool);

 protected:
  static double getTestTime();

  std::unique_ptr<base::SimpleTestTickClock> m_clock;
  scoped_refptr<cc::OrderedSimpleTaskRunner> m_mockTaskRunner;
  std::unique_ptr<scheduler::RendererSchedulerImpl> m_scheduler;
  std::unique_ptr<WebThread> m_thread;
};

class ScopedUnittestsEnvironmentSetup {
  WTF_MAKE_NONCOPYABLE(ScopedUnittestsEnvironmentSetup);

 public:
  ScopedUnittestsEnvironmentSetup(int argc, char** argv);
  ~ScopedUnittestsEnvironmentSetup();

 private:
  class DummyPlatform;
  std::unique_ptr<base::TestDiscardableMemoryAllocator>
      m_discardableMemoryAllocator;
  std::unique_ptr<DummyPlatform> m_platform;
  std::unique_ptr<cc_blink::WebCompositorSupportImpl> m_compositorSupport;
  TestingPlatformSupport::Config m_testingPlatformConfig;
  std::unique_ptr<TestingPlatformSupport> m_testingPlatformSupport;
};

}  // namespace blink

#endif  // TestingPlatformSupport_h
