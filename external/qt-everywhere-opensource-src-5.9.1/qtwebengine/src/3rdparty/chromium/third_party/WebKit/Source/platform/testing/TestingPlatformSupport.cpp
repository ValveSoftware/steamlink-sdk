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

#include "platform/testing/TestingPlatformSupport.h"

#include "base/command_line.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/icu_test_util.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "cc/blink/web_compositor_support_impl.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "platform/HTTPNames.h"
#include "platform/heap/Heap.h"
#include "platform/network/mime/MockMimeRegistry.h"
#include "platform/scheduler/base/real_time_domain.h"
#include "platform/scheduler/base/task_queue_manager.h"
#include "platform/scheduler/base/test_time_source.h"
#include "platform/scheduler/child/scheduler_tqm_delegate_for_test.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/WebContentLayer.h"
#include "public/platform/WebExternalTextureLayer.h"
#include "public/platform/WebImageLayer.h"
#include "public/platform/WebScrollbarLayer.h"
#include "wtf/CryptographicallyRandomNumber.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"
#include "wtf/WTF.h"
#include "wtf/allocator/Partitions.h"
#include <memory>

namespace blink {

class TestingPlatformSupport::TestingInterfaceProvider
    : public blink::InterfaceProvider {
 public:
  TestingInterfaceProvider() = default;
  virtual ~TestingInterfaceProvider() = default;

  void getInterface(const char* name,
                    mojo::ScopedMessagePipeHandle handle) override {
    if (std::string(name) == mojom::blink::MimeRegistry::Name_) {
      mojo::MakeStrongBinding(
          makeUnique<MockMimeRegistry>(),
          mojo::MakeRequest<mojom::blink::MimeRegistry>(std::move(handle)));
      return;
    }
  }
};

namespace {

double dummyCurrentTime() {
  return 0.0;
}

class DummyThread final : public blink::WebThread {
 public:
  bool isCurrentThread() const override { return true; }
  blink::WebScheduler* scheduler() const override { return nullptr; }
};

}  // namespace

std::unique_ptr<WebLayer> TestingCompositorSupport::createLayer() {
  return nullptr;
}

std::unique_ptr<WebLayer> TestingCompositorSupport::createLayerFromCCLayer(
    cc::Layer*) {
  return nullptr;
}

std::unique_ptr<WebContentLayer> TestingCompositorSupport::createContentLayer(
    WebContentLayerClient*) {
  return nullptr;
}
std::unique_ptr<WebExternalTextureLayer>
TestingCompositorSupport::createExternalTextureLayer(cc::TextureLayerClient*) {
  return nullptr;
}

std::unique_ptr<WebImageLayer> TestingCompositorSupport::createImageLayer() {
  return nullptr;
}

std::unique_ptr<WebScrollbarLayer>
TestingCompositorSupport::createScrollbarLayer(
    std::unique_ptr<WebScrollbar>,
    WebScrollbarThemePainter,
    std::unique_ptr<WebScrollbarThemeGeometry>) {
  return nullptr;
}

std::unique_ptr<WebScrollbarLayer>
TestingCompositorSupport::createSolidColorScrollbarLayer(
    WebScrollbar::Orientation,
    int thumbThickness,
    int trackStart,
    bool isLeftSideVerticalScrollbar) {
  return nullptr;
}

TestingPlatformSupport::TestingPlatformSupport()
    : TestingPlatformSupport(TestingPlatformSupport::Config()) {}

TestingPlatformSupport::TestingPlatformSupport(const Config& config)
    : m_config(config),
      m_oldPlatform(Platform::current()),
      m_interfaceProvider(new TestingInterfaceProvider) {
  ASSERT(m_oldPlatform);
  Platform::setCurrentPlatformForTesting(this);
}

TestingPlatformSupport::~TestingPlatformSupport() {
  Platform::setCurrentPlatformForTesting(m_oldPlatform);
}

WebString TestingPlatformSupport::defaultLocale() {
  return WebString::fromUTF8("en-US");
}

WebCompositorSupport* TestingPlatformSupport::compositorSupport() {
  if (m_config.compositorSupport)
    return m_config.compositorSupport;

  return m_oldPlatform ? m_oldPlatform->compositorSupport() : nullptr;
}

WebThread* TestingPlatformSupport::currentThread() {
  return m_oldPlatform ? m_oldPlatform->currentThread() : nullptr;
}

WebBlobRegistry* TestingPlatformSupport::getBlobRegistry() {
  return m_oldPlatform ? m_oldPlatform->getBlobRegistry() : nullptr;
}

WebClipboard* TestingPlatformSupport::clipboard() {
  return m_oldPlatform ? m_oldPlatform->clipboard() : nullptr;
}

WebFileUtilities* TestingPlatformSupport::fileUtilities() {
  return m_oldPlatform ? m_oldPlatform->fileUtilities() : nullptr;
}

WebIDBFactory* TestingPlatformSupport::idbFactory() {
  return m_oldPlatform ? m_oldPlatform->idbFactory() : nullptr;
}

WebURLLoaderMockFactory* TestingPlatformSupport::getURLLoaderMockFactory() {
  return m_oldPlatform ? m_oldPlatform->getURLLoaderMockFactory() : nullptr;
}

WebURLLoader* TestingPlatformSupport::createURLLoader() {
  return m_oldPlatform ? m_oldPlatform->createURLLoader() : nullptr;
}

WebData TestingPlatformSupport::loadResource(const char* name) {
  return m_oldPlatform ? m_oldPlatform->loadResource(name) : WebData();
}

WebURLError TestingPlatformSupport::cancelledError(const WebURL& url) const {
  return m_oldPlatform ? m_oldPlatform->cancelledError(url) : WebURLError();
}

InterfaceProvider* TestingPlatformSupport::interfaceProvider() {
  return m_interfaceProvider.get();
}

// TestingPlatformSupportWithMockScheduler definition:

TestingPlatformSupportWithMockScheduler::
    TestingPlatformSupportWithMockScheduler()
    : TestingPlatformSupportWithMockScheduler(
          TestingPlatformSupport::Config()) {}

TestingPlatformSupportWithMockScheduler::
    TestingPlatformSupportWithMockScheduler(const Config& config)
    : TestingPlatformSupport(config),
      m_clock(new base::SimpleTestTickClock()),
      m_mockTaskRunner(new cc::OrderedSimpleTaskRunner(m_clock.get(), true)),
      m_scheduler(new scheduler::RendererSchedulerImpl(
          scheduler::SchedulerTqmDelegateForTest::Create(
              m_mockTaskRunner,
              base::WrapUnique(new scheduler::TestTimeSource(m_clock.get()))))),
      m_thread(m_scheduler->CreateMainThread()) {
  // Set the work batch size to one so RunPendingTasks behaves as expected.
  m_scheduler->GetSchedulerHelperForTesting()->SetWorkBatchSizeForTesting(1);

  WTF::setTimeFunctionsForTesting(getTestTime);
}

TestingPlatformSupportWithMockScheduler::
    ~TestingPlatformSupportWithMockScheduler() {
  WTF::setTimeFunctionsForTesting(nullptr);
  m_scheduler->Shutdown();
}

WebThread* TestingPlatformSupportWithMockScheduler::currentThread() {
  if (m_thread->isCurrentThread())
    return m_thread.get();
  return TestingPlatformSupport::currentThread();
}

void TestingPlatformSupportWithMockScheduler::runSingleTask() {
  m_mockTaskRunner->SetRunTaskLimit(1);
  m_mockTaskRunner->RunPendingTasks();
  m_mockTaskRunner->ClearRunTaskLimit();
}

void TestingPlatformSupportWithMockScheduler::runUntilIdle() {
  m_mockTaskRunner->RunUntilIdle();
}

void TestingPlatformSupportWithMockScheduler::runForPeriodSeconds(
    double seconds) {
  const base::TimeTicks deadline =
      m_clock->NowTicks() + base::TimeDelta::FromSecondsD(seconds);

  scheduler::TaskQueueManager* taskQueueManager =
      m_scheduler->GetSchedulerHelperForTesting()
          ->GetTaskQueueManagerForTesting();

  for (;;) {
    // If we've run out of immediate work then fast forward to the next delayed
    // task, but don't pass |deadline|.
    if (!taskQueueManager->HasImmediateWorkForTesting()) {
      base::TimeTicks nextDelayedTask;
      if (!taskQueueManager->real_time_domain()->NextScheduledRunTime(
              &nextDelayedTask) ||
          nextDelayedTask > deadline) {
        break;
      }

      m_clock->SetNowTicks(nextDelayedTask);
    }

    if (m_clock->NowTicks() > deadline)
      break;

    m_mockTaskRunner->RunPendingTasks();
  }

  m_clock->SetNowTicks(deadline);
}

void TestingPlatformSupportWithMockScheduler::advanceClockSeconds(
    double seconds) {
  m_clock->Advance(base::TimeDelta::FromSecondsD(seconds));
}

void TestingPlatformSupportWithMockScheduler::setAutoAdvanceNowToPendingTasks(
    bool autoAdvance) {
  m_mockTaskRunner->SetAutoAdvanceNowToPendingTasks(autoAdvance);
}

scheduler::RendererScheduler*
TestingPlatformSupportWithMockScheduler::rendererScheduler() const {
  return m_scheduler.get();
}

// static
double TestingPlatformSupportWithMockScheduler::getTestTime() {
  TestingPlatformSupportWithMockScheduler* platform =
      static_cast<TestingPlatformSupportWithMockScheduler*>(
          Platform::current());
  return (platform->m_clock->NowTicks() - base::TimeTicks()).InSecondsF();
}

class ScopedUnittestsEnvironmentSetup::DummyPlatform final
    : public blink::Platform {
 public:
  DummyPlatform() {}

  blink::WebThread* currentThread() override {
    static DummyThread dummyThread;
    return &dummyThread;
  };
};

ScopedUnittestsEnvironmentSetup::ScopedUnittestsEnvironmentSetup(int argc,
                                                                 char** argv) {
  base::CommandLine::Init(argc, argv);

  base::test::InitializeICUForTesting();

  m_discardableMemoryAllocator =
      wrapUnique(new base::TestDiscardableMemoryAllocator);
  base::DiscardableMemoryAllocator::SetInstance(
      m_discardableMemoryAllocator.get());
  base::StatisticsRecorder::Initialize();

  m_platform = wrapUnique(new DummyPlatform);
  Platform::setCurrentPlatformForTesting(m_platform.get());

  WTF::Partitions::initialize(nullptr);
  WTF::setTimeFunctionsForTesting(dummyCurrentTime);
  WTF::initialize(nullptr);

  m_compositorSupport = wrapUnique(new cc_blink::WebCompositorSupportImpl);
  m_testingPlatformConfig.compositorSupport = m_compositorSupport.get();
  m_testingPlatformSupport =
      makeUnique<TestingPlatformSupport>(m_testingPlatformConfig);

  ProcessHeap::init();
  ThreadState::attachMainThread();
  ThreadState::current()->registerTraceDOMWrappers(nullptr, nullptr, nullptr,
                                                   nullptr);
  HTTPNames::init();
}

ScopedUnittestsEnvironmentSetup::~ScopedUnittestsEnvironmentSetup() {}

}  // namespace blink
