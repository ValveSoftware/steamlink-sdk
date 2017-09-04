// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/feedback/blimp_feedback_data.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "blimp/client/core/compositor/blimp_compositor_dependencies.h"
#include "blimp/client/core/contents/blimp_contents_impl.h"
#include "blimp/client/core/contents/blimp_contents_manager.h"
#include "blimp/client/core/contents/ime_feature.h"
#include "blimp/client/core/contents/tab_control_feature.h"
#include "blimp/client/core/render_widget/render_widget_feature.h"
#include "blimp/client/test/compositor/mock_compositor_dependencies.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_ANDROID)
#include "ui/android/window_android.h"
#endif  // defined(OS_ANDROID)

const char kDefaultUserName[] = "mock_user";

namespace blimp {
namespace client {
namespace {

class MockTabControlFeature : public TabControlFeature {
 public:
  MockTabControlFeature() {}
  ~MockTabControlFeature() override = default;

  MOCK_METHOD1(CreateTab, void(int));
  MOCK_METHOD1(CloseTab, void(int));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTabControlFeature);
};

class BlimpFeedbackDataTest : public testing::Test {
 public:
  BlimpFeedbackDataTest()
      : compositor_deps_(base::MakeUnique<MockCompositorDependencies>()),
        blimp_contents_manager_(&compositor_deps_,
                                &ime_feature_,
                                nullptr,
                                &render_widget_feature_,
                                &tab_control_feature_) {}

#if defined(OS_ANDROID)
  void SetUp() override { window_ = ui::WindowAndroid::CreateForTesting(); }

  void TearDown() override { window_->DestroyForTesting(); }
#endif  // defined(OS_ANDROID)

 protected:
  gfx::NativeWindow window_ = nullptr;

  base::MessageLoop loop_;
  ImeFeature ime_feature_;
  RenderWidgetFeature render_widget_feature_;
  MockTabControlFeature tab_control_feature_;
  BlimpCompositorDependencies compositor_deps_;
  BlimpContentsManager blimp_contents_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpFeedbackDataTest);
};

TEST_F(BlimpFeedbackDataTest, IncludesBlimpIsSupported) {
  std::unordered_map<std::string, std::string> data =
      CreateBlimpFeedbackData(&blimp_contents_manager_, kDefaultUserName);
  auto search = data.find(kFeedbackSupportedKey);
  ASSERT_TRUE(search != data.end());
  EXPECT_EQ("true", search->second);
}

TEST_F(BlimpFeedbackDataTest, CheckVisibilityCalculation) {
  EXPECT_CALL(tab_control_feature_, CreateTab(testing::_)).Times(1);
  std::unique_ptr<BlimpContentsImpl> blimp_contents =
      blimp_contents_manager_.CreateBlimpContents(window_);

  // Verify that visibility is false when there are no visible BlimpContents.
  blimp_contents->Hide();
  std::unordered_map<std::string, std::string> data =
      CreateBlimpFeedbackData(&blimp_contents_manager_, kDefaultUserName);
  auto search = data.find(kFeedbackHasVisibleBlimpContents);
  ASSERT_TRUE(search != data.end());
  EXPECT_EQ("false", search->second);

  // Verify that visibility is true when there are visible BlimpContents.
  blimp_contents->Show();
  data = CreateBlimpFeedbackData(&blimp_contents_manager_, kDefaultUserName);
  search = data.find(kFeedbackHasVisibleBlimpContents);
  ASSERT_TRUE(search != data.end());
  EXPECT_EQ("true", search->second);
}

TEST_F(BlimpFeedbackDataTest, CheckUserName) {
  // Verify non-empty user name in the feedback data.
  std::unordered_map<std::string, std::string> data =
      CreateBlimpFeedbackData(&blimp_contents_manager_, kDefaultUserName);
  auto search = data.find(kFeedbackUserNameKey);
  ASSERT_TRUE(search != data.end());
  EXPECT_EQ(kDefaultUserName, search->second);
}

TEST_F(BlimpFeedbackDataTest, CheckEmptyUserName) {
  // Verify empty user name in the feedback data.
  std::unordered_map<std::string, std::string> data =
      CreateBlimpFeedbackData(&blimp_contents_manager_, "");
  auto search = data.find(kFeedbackUserNameKey);
  ASSERT_TRUE(search != data.end());
  EXPECT_EQ("", search->second);
}

}  // namespace
}  // namespace client
}  // namespace blimp
