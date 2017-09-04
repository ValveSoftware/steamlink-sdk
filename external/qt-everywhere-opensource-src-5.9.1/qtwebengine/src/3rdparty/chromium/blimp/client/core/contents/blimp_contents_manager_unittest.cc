// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/contents/blimp_contents_manager.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "blimp/client/core/compositor/blimp_compositor_dependencies.h"
#include "blimp/client/core/contents/blimp_contents_impl.h"
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

using testing::_;

namespace {
const int kDummyBlimpContentsId = 0;
}

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

class BlimpContentsManagerTest : public testing::Test {
 public:
  BlimpContentsManagerTest()
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
  DISALLOW_COPY_AND_ASSIGN(BlimpContentsManagerTest);
};

TEST_F(BlimpContentsManagerTest, GetExistingBlimpContents) {
  EXPECT_CALL(tab_control_feature_, CreateTab(_)).Times(1);
  std::unique_ptr<BlimpContentsImpl> blimp_contents =
      blimp_contents_manager_.CreateBlimpContents(window_);
  int id = blimp_contents->id();
  BlimpContentsImpl* existing_contents =
      blimp_contents_manager_.GetBlimpContents(id);
  EXPECT_EQ(blimp_contents.get(), existing_contents);
}

TEST_F(BlimpContentsManagerTest, GetNonExistingBlimpContents) {
  BlimpContentsImpl* existing_contents =
      blimp_contents_manager_.GetBlimpContents(kDummyBlimpContentsId);
  EXPECT_EQ(nullptr, existing_contents);
}

TEST_F(BlimpContentsManagerTest, GetDestroyedBlimpContents) {
  EXPECT_CALL(tab_control_feature_, CreateTab(_)).Times(1);
  std::unique_ptr<BlimpContentsImpl> blimp_contents =
      blimp_contents_manager_.CreateBlimpContents(window_);
  int id = blimp_contents.get()->id();
  BlimpContentsImpl* existing_contents =
      blimp_contents_manager_.GetBlimpContents(id);
  EXPECT_EQ(blimp_contents.get(), existing_contents);

  EXPECT_CALL(tab_control_feature_, CloseTab(id)).Times(1);
  blimp_contents.reset();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, blimp_contents_manager_.GetBlimpContents(id));
}

// TODO(mlliu): Increase the number of BlimpContentsImpl in this test case.
// (See http://crbug.com/642558)
TEST_F(BlimpContentsManagerTest, RetrieveAllBlimpContents) {
  EXPECT_CALL(tab_control_feature_, CreateTab(_)).Times(1);
  std::unique_ptr<BlimpContentsImpl> blimp_contents =
      blimp_contents_manager_.CreateBlimpContents(window_);
  int created_id = blimp_contents->id();

  std::vector<BlimpContentsImpl*> all_blimp_contents =
      blimp_contents_manager_.GetAllActiveBlimpContents();
  ASSERT_EQ(1U, all_blimp_contents.size());
  EXPECT_EQ(created_id, (*all_blimp_contents.begin())->id());
}

// TODO(mlliu): Increase the number of BlimpContentsImpl in this test case.
// (See http://crbug.com/642558)
TEST_F(BlimpContentsManagerTest, NoRetrievedBlimpContentsAreDestroyed) {
  EXPECT_CALL(tab_control_feature_, CreateTab(_)).Times(1);
  std::unique_ptr<BlimpContentsImpl> blimp_contents =
      blimp_contents_manager_.CreateBlimpContents(window_);
  blimp_contents.reset();

  std::vector<BlimpContentsImpl*> all_blimp_contents =
      blimp_contents_manager_.GetAllActiveBlimpContents();
  EXPECT_EQ(0U, all_blimp_contents.size());
}

// TODO(mlliu): remove this test case (http://crbug.com/642558)
TEST_F(BlimpContentsManagerTest, CreateTwoBlimpContentsDestroyAndCreate) {
  EXPECT_CALL(tab_control_feature_, CreateTab(_)).Times(2);
  std::unique_ptr<BlimpContentsImpl> blimp_contents =
      blimp_contents_manager_.CreateBlimpContents(window_);
  EXPECT_NE(blimp_contents, nullptr);

  std::unique_ptr<BlimpContentsImpl> second_blimp_contents =
      blimp_contents_manager_.CreateBlimpContents(window_);
  EXPECT_EQ(second_blimp_contents, nullptr);

  blimp_contents.reset();
  std::unique_ptr<BlimpContentsImpl> third_blimp_contents =
      blimp_contents_manager_.CreateBlimpContents(window_);
  EXPECT_NE(third_blimp_contents, nullptr);
}

}  // namespace
}  // namespace client
}  // namespace blimp
