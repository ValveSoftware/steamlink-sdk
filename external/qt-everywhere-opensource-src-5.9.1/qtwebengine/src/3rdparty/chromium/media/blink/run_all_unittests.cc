// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "media/base/media.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/scheduler/renderer/renderer_scheduler.h"
#include "third_party/WebKit/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/WebKit/public/web/WebKit.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "media/base/android/media_jni_registrar.h"
#endif

#if !defined(OS_IOS)
#include "mojo/edk/embedder/embedder.h"
#endif

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
#include "gin/v8_initializer.h"
#endif

class TestBlinkPlatformSupport : public blink::Platform {
 public:
  TestBlinkPlatformSupport()
      : renderer_scheduler_(
            blink::scheduler::CreateRendererSchedulerForTests()),
        main_thread_(renderer_scheduler_->CreateMainThread()) {}
  ~TestBlinkPlatformSupport() override;

  blink::WebThread* currentThread() override {
    EXPECT_TRUE(main_thread_->isCurrentThread());
    return main_thread_.get();
  }

 private:
  std::unique_ptr<blink::scheduler::RendererScheduler> renderer_scheduler_;
  std::unique_ptr<blink::WebThread> main_thread_;
};

TestBlinkPlatformSupport::~TestBlinkPlatformSupport() {
  renderer_scheduler_->Shutdown();
}

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

// Initialize mojo firstly to enable Blink initialization to use it.
#if !defined(OS_IOS)
  mojo::edk::Init();
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
