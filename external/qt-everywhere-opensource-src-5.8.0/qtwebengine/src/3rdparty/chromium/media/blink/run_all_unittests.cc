// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "components/scheduler/child/scheduler_tqm_delegate_impl.h"
#include "components/scheduler/child/web_task_runner_impl.h"
#include "components/scheduler/renderer/renderer_scheduler_impl.h"
#include "components/scheduler/renderer/renderer_web_scheduler_impl.h"
#include "components/scheduler/test/lazy_scheduler_message_loop_delegate_for_tests.h"
#include "media/base/media.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebScheduler.h"
#include "third_party/WebKit/public/platform/WebTaskRunner.h"
#include "third_party/WebKit/public/platform/WebThread.h"
#include "third_party/WebKit/public/web/WebKit.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "media/base/android/media_jni_registrar.h"
#endif

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
#include "gin/v8_initializer.h"
#endif

class CurrentThreadMock : public blink::WebThread {
 public:
  CurrentThreadMock()
      : task_runner_delegate_(
            scheduler::LazySchedulerMessageLoopDelegateForTests::Create()),
        scheduler_(
            new scheduler::RendererSchedulerImpl(task_runner_delegate_.get())),
        web_scheduler_(
            new scheduler::RendererWebSchedulerImpl(scheduler_.get())),
        web_task_runner_(
            new scheduler::WebTaskRunnerImpl(scheduler_->DefaultTaskRunner())) {
  }

  ~CurrentThreadMock() override {
    scheduler_->Shutdown();
  }

  blink::WebTaskRunner* getWebTaskRunner() override {
    return web_task_runner_.get();
  }

  bool isCurrentThread() const override { return true; }

  blink::PlatformThreadId threadId() const override { return 17; }

  blink::WebScheduler* scheduler() const override {
    return web_scheduler_.get();
  }

 private:
  scoped_refptr<scheduler::SchedulerTqmDelegate> task_runner_delegate_;
  std::unique_ptr<scheduler::RendererSchedulerImpl> scheduler_;
  std::unique_ptr<blink::WebScheduler> web_scheduler_;
  std::unique_ptr<blink::WebTaskRunner> web_task_runner_;
};

class TestBlinkPlatformSupport : NON_EXPORTED_BASE(public blink::Platform) {
 public:
  ~TestBlinkPlatformSupport() override;

  blink::WebThread* currentThread() override { return &m_currentThread; }

 private:
  CurrentThreadMock m_currentThread;
};

TestBlinkPlatformSupport::~TestBlinkPlatformSupport() {}

class BlinkMediaTestSuite : public base::TestSuite {
 public:
  BlinkMediaTestSuite(int argc, char** argv);
  ~BlinkMediaTestSuite() override;

 protected:
  void Initialize() override;

 private:
  std::unique_ptr<TestBlinkPlatformSupport> blink_platform_support_;
};

BlinkMediaTestSuite::BlinkMediaTestSuite(int argc, char** argv)
    : TestSuite(argc, argv),
      blink_platform_support_(new TestBlinkPlatformSupport()) {
}

BlinkMediaTestSuite::~BlinkMediaTestSuite() {}

void BlinkMediaTestSuite::Initialize() {
  // Run TestSuite::Initialize first so that logging is initialized.
  base::TestSuite::Initialize();

#if defined(OS_ANDROID)
  media::RegisterJni(base::android::AttachCurrentThread());
#endif

  // Run this here instead of main() to ensure an AtExitManager is already
  // present.
  media::InitializeMediaLibrary();

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  gin::V8Initializer::LoadV8Snapshot();
  gin::V8Initializer::LoadV8Natives();
#endif

  // Dummy task runner is initialized here because the blink::initialize creates
  // IsolateHolder which needs the current task runner handle. There should be
  // no task posted to this task runner.
  std::unique_ptr<base::MessageLoop> message_loop;
  if (!base::MessageLoop::current())
    message_loop.reset(new base::MessageLoop());
  blink::initialize(blink_platform_support_.get());
}

int main(int argc, char** argv) {
  BlinkMediaTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv, base::Bind(&BlinkMediaTestSuite::Run,
                             base::Unretained(&test_suite)));
}
