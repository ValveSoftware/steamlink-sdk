// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/public/contents/blimp_contents_observer.h"

#include "base/memory/ptr_util.h"
#include "blimp/client/core/compositor/blimp_compositor_dependencies.h"
#include "blimp/client/core/contents/blimp_contents_impl.h"
#include "blimp/client/core/contents/ime_feature.h"
#include "blimp/client/core/render_widget/render_widget_feature.h"
#include "blimp/client/test/compositor/mock_compositor_dependencies.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_ANDROID)
#include "ui/android/window_android.h"
#endif  // defined(OS_ANDROID)

namespace {
const int kDummyBlimpContentsId = 0;
}

namespace blimp {
namespace client {

namespace {

class TestBlimpContentsObserver : public BlimpContentsObserver {
 public:
  explicit TestBlimpContentsObserver(BlimpContents* blimp_contents)
      : BlimpContentsObserver(blimp_contents) {}

  MOCK_METHOD0(OnContentsDestroyed, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBlimpContentsObserver);
};

class BlimpContentsObserverTest : public testing::Test {
 public:
  BlimpContentsObserverTest() = default;

#if defined(OS_ANDROID)
  void SetUp() override { window_ = ui::WindowAndroid::CreateForTesting(); }

  void TearDown() override { window_->DestroyForTesting(); }
#endif  // defined(OS_ANDROID)

 protected:
  gfx::NativeWindow window_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpContentsObserverTest);
};

TEST_F(BlimpContentsObserverTest, ObserverDies) {
  ImeFeature ime_feature;
  RenderWidgetFeature render_widget_feature;
  BlimpCompositorDependencies compositor_deps(
      base::MakeUnique<MockCompositorDependencies>());
  BlimpContentsImpl contents(kDummyBlimpContentsId, window_, &compositor_deps,
                             &ime_feature, nullptr, &render_widget_feature,
                             nullptr);

  std::unique_ptr<BlimpContentsObserver> observer =
      base::MakeUnique<TestBlimpContentsObserver>(&contents);
  BlimpContentsObserver* observer_ptr = observer.get();
  EXPECT_TRUE(contents.HasObserver(observer_ptr));
  observer.reset();

  EXPECT_FALSE(contents.HasObserver(observer_ptr));
}

TEST_F(BlimpContentsObserverTest, ContentsDies) {
  std::unique_ptr<TestBlimpContentsObserver> observer;
  ImeFeature ime_feature;
  RenderWidgetFeature render_widget_feature;
  BlimpCompositorDependencies compositor_deps(
      base::MakeUnique<MockCompositorDependencies>());
  std::unique_ptr<BlimpContentsImpl> contents =
      base::MakeUnique<BlimpContentsImpl>(
          kDummyBlimpContentsId, window_, &compositor_deps, &ime_feature,
          nullptr, &render_widget_feature, nullptr);
  observer.reset(new TestBlimpContentsObserver(contents.get()));
  EXPECT_CALL(*observer, OnContentsDestroyed()).Times(1);
  EXPECT_EQ(observer->blimp_contents(), contents.get());
  contents.reset();

  EXPECT_EQ(observer->blimp_contents(), nullptr);
}

}  // namespace

}  // namespace client
}  // namespace blimp
