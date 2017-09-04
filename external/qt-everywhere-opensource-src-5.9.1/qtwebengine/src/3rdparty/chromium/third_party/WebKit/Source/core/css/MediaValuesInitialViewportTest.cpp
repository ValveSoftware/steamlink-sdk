// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/MediaValuesInitialViewport.h"

#include "core/frame/FrameView.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MediaValuesInitialViewportTest : public ::testing::Test {
 protected:
  Document& document() const { return m_dummyPageHolder->document(); }

 private:
  void SetUp() override {
    m_dummyPageHolder = DummyPageHolder::create(IntSize(320, 480));
    document().view()->setInitialViewportSize(IntSize(320, 480));
  }

  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

TEST_F(MediaValuesInitialViewportTest, InitialViewportSize) {
  FrameView* view = document().view();
  ASSERT_TRUE(view);
  EXPECT_TRUE(view->layoutSizeFixedToFrameSize());

  MediaValues* mediaValues =
      MediaValuesInitialViewport::create(*document().frame());
  EXPECT_EQ(320, mediaValues->viewportWidth());
  EXPECT_EQ(480, mediaValues->viewportHeight());

  view->setLayoutSizeFixedToFrameSize(false);
  view->setLayoutSize(IntSize(800, 600));
  EXPECT_EQ(320, mediaValues->viewportWidth());
  EXPECT_EQ(480, mediaValues->viewportHeight());
}

}  // namespace blink
