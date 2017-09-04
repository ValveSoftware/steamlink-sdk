// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/context/blimp_client_context_impl.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "blimp/client/core/contents/blimp_contents_impl.h"
#include "blimp/client/core/contents/tab_control_feature.h"
#include "blimp/client/core/settings/settings.h"
#include "blimp/client/public/blimp_client_context_delegate.h"
#include "blimp/client/public/contents/blimp_contents.h"
#include "blimp/client/test/compositor/mock_compositor_dependencies.h"
#include "blimp/client/test/test_blimp_client_context_delegate.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_ANDROID)
#include "blimp/client/core/settings/android/settings_android.h"
#include "ui/android/window_android.h"
#endif  // defined(OS_ANDROID)

namespace blimp {
namespace client {
namespace {

class BlimpClientContextImplTest : public testing::Test {
 public:
  BlimpClientContextImplTest() : io_thread_("BlimpTestIO") {}
  ~BlimpClientContextImplTest() override {}

  void SetUp() override {
    base::Thread::Options options;
    options.message_loop_type = base::MessageLoop::TYPE_IO;
    io_thread_.StartWithOptions(options);
#if defined(OS_ANDROID)
    window_ = ui::WindowAndroid::CreateForTesting();
#endif  // defined(OS_ANDROID)
  }

  void TearDown() override {
    io_thread_.Stop();
    base::RunLoop().RunUntilIdle();
#if defined(OS_ANDROID)
    window_->DestroyForTesting();
#endif  // defined(OS_ANDROID)
  }

 protected:
  base::Thread io_thread_;
  gfx::NativeWindow window_ = nullptr;

 private:
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(BlimpClientContextImplTest);
};

TEST_F(BlimpClientContextImplTest,
       CreatedBlimpContentsGetsHelpersAttachedAndHasTabControlFeature) {
  TestingPrefServiceSimple prefs;
  Settings::RegisterPrefs(prefs.registry());

  auto settings = base::MakeUnique<Settings>(&prefs);
  BlimpClientContextImpl blimp_client_context(
      io_thread_.task_runner(), io_thread_.task_runner(),
      base::MakeUnique<MockCompositorDependencies>(), std::move(settings));
  TestBlimpClientContextDelegate delegate;
  blimp_client_context.SetDelegate(&delegate);

  BlimpContents* attached_blimp_contents = nullptr;

  EXPECT_CALL(delegate, AttachBlimpContentsHelpers(testing::_))
      .WillOnce(testing::SaveArg<0>(&attached_blimp_contents))
      .RetiresOnSaturation();

  std::unique_ptr<BlimpContents> blimp_contents =
      blimp_client_context.CreateBlimpContents(window_);
  DCHECK(blimp_contents);
  DCHECK_EQ(blimp_contents.get(), attached_blimp_contents);
  blimp_client_context.SetDelegate(nullptr);
}

}  // namespace
}  // namespace client
}  // namespace blimp
