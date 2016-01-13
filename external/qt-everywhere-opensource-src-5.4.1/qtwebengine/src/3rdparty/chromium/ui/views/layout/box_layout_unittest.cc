// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/box_layout.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/test_views.h"
#include "ui/views/view.h"

namespace views {

namespace {

class BoxLayoutTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    host_.reset(new View);
  }

  scoped_ptr<View> host_;
  scoped_ptr<BoxLayout> layout_;
};

}  // namespace

TEST_F(BoxLayoutTest, Empty) {
  layout_.reset(new BoxLayout(BoxLayout::kHorizontal, 10, 10, 20));
  EXPECT_EQ(gfx::Size(20, 20), layout_->GetPreferredSize(host_.get()));
}

TEST_F(BoxLayoutTest, AlignmentHorizontal) {
  layout_.reset(new BoxLayout(BoxLayout::kHorizontal, 0, 0, 0));
  View* v1 = new StaticSizedView(gfx::Size(10, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(20, 20), layout_->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 20, 20);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(0, 0, 10, 20), v1->bounds());
  EXPECT_EQ(gfx::Rect(10, 0, 10, 20), v2->bounds());
}

TEST_F(BoxLayoutTest, AlignmentVertical) {
  layout_.reset(new BoxLayout(BoxLayout::kVertical, 0, 0, 0));
  View* v1 = new StaticSizedView(gfx::Size(20, 10));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(20, 20), layout_->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 20, 20);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(0, 0, 20, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(0, 10, 20, 10), v2->bounds());
}

TEST_F(BoxLayoutTest, SetInsideBorderInsets) {
  layout_.reset(new BoxLayout(BoxLayout::kHorizontal, 10, 20, 0));
  View* v1 = new StaticSizedView(gfx::Size(10, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(40, 60), layout_->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 40, 60);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(10, 20, 10, 20), v1->bounds());
  EXPECT_EQ(gfx::Rect(20, 20, 10, 20), v2->bounds());

  layout_->set_inside_border_insets(
      gfx::Insets(5, 10, 15, 20));
  EXPECT_EQ(gfx::Size(50, 40), layout_->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 50, 40);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(10, 5, 10, 20), v1->bounds());
  EXPECT_EQ(gfx::Rect(20, 5, 10, 20), v2->bounds());
}

TEST_F(BoxLayoutTest, Spacing) {
  layout_.reset(new BoxLayout(BoxLayout::kHorizontal, 7, 7, 8));
  View* v1 = new StaticSizedView(gfx::Size(10, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 20));
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(42, 34), layout_->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 100, 100);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(7, 7, 10, 86), v1->bounds());
  EXPECT_EQ(gfx::Rect(25, 7, 10, 86), v2->bounds());
}

TEST_F(BoxLayoutTest, Overflow) {
  layout_.reset(new BoxLayout(BoxLayout::kHorizontal, 0, 0, 0));
  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 20));
  host_->AddChildView(v2);
  host_->SetBounds(0, 0, 10, 10);

  // Overflows by positioning views at the start and truncating anything that
  // doesn't fit.
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), v2->bounds());

  // All values of main axis alignment should overflow in the same manner.
  layout_->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), v2->bounds());

  layout_->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), v2->bounds());

  layout_->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_END);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), v2->bounds());
}

TEST_F(BoxLayoutTest, NoSpace) {
  layout_.reset(new BoxLayout(BoxLayout::kHorizontal, 10, 10, 10));
  View* childView = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(childView);
  host_->SetBounds(0, 0, 10, 10);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), childView->bounds());
}

TEST_F(BoxLayoutTest, InvisibleChild) {
  layout_.reset(new BoxLayout(BoxLayout::kHorizontal, 10, 10, 10));
  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  v1->SetVisible(false);
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(30, 30), layout_->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 30, 30);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(10, 10, 10, 10), v2->bounds());
}

TEST_F(BoxLayoutTest, MainAxisAlignmentFill) {
  layout_.reset(new BoxLayout(BoxLayout::kHorizontal, 10, 10, 10));
  layout_->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_FILL);

  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(60, 40), layout_->GetPreferredSize(host_.get()));

  host_->SetBounds(0, 0, 100, 40);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(10, 10, 40, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(60, 10, 30, 20).ToString(), v2->bounds().ToString());
}

TEST_F(BoxLayoutTest, UseHeightForWidth) {
  layout_.reset(new BoxLayout(BoxLayout::kVertical, 0, 0, 0));
  View* v1 = new StaticSizedView(gfx::Size(20, 10));
  host_->AddChildView(v1);
  View* v2 = new ProportionallySizedView(2);
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(20, 50), layout_->GetPreferredSize(host_.get()));

  host_->SetBounds(0, 0, 20, 50);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(0, 0, 20, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(0, 10, 20, 40), v2->bounds());

  EXPECT_EQ(110, layout_->GetPreferredHeightForWidth(host_.get(), 50));
}

TEST_F(BoxLayoutTest, EmptyPreferredSize) {
  for (size_t i = 0; i < 2; i++) {
    BoxLayout::Orientation orientation = i == 0 ? BoxLayout::kHorizontal :
                                                  BoxLayout::kVertical;
    host_->RemoveAllChildViews(true);
    host_->SetLayoutManager(new BoxLayout(orientation, 0, 0, 5));
    View* v1 = new StaticSizedView(gfx::Size());
    host_->AddChildView(v1);
    View* v2 = new StaticSizedView(gfx::Size(10, 10));
    host_->AddChildView(v2);
    host_->SizeToPreferredSize();
    host_->Layout();

    EXPECT_EQ(v2->GetPreferredSize().width(), host_->bounds().width()) << i;
    EXPECT_EQ(v2->GetPreferredSize().height(), host_->bounds().height()) << i;
    EXPECT_EQ(v1->GetPreferredSize().width(), v1->bounds().width()) << i;
    EXPECT_EQ(v1->GetPreferredSize().height(), v1->bounds().height()) << i;
    EXPECT_EQ(v2->GetPreferredSize().width(), v2->bounds().width()) << i;
    EXPECT_EQ(v2->GetPreferredSize().height(), v2->bounds().height()) << i;
  }
}

TEST_F(BoxLayoutTest, MainAxisAlignmentHorizontal) {
  layout_.reset(new BoxLayout(BoxLayout::kHorizontal, 10, 10, 10));

  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);

  host_->SetBounds(0, 0, 100, 40);

  // Align children to the horizontal start by default.
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(40, 10, 10, 20).ToString(), v2->bounds().ToString());

  // Ensure same results for MAIN_AXIS_ALIGNMENT_START.
  layout_->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(40, 10, 10, 20).ToString(), v2->bounds().ToString());

  // Aligns children to the center horizontally.
  layout_->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(30, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(60, 10, 10, 20).ToString(), v2->bounds().ToString());

  // Aligns children to the end of the host horizontally, accounting for the
  // inside border spacing.
  layout_->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_END);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(50, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(80, 10, 10, 20).ToString(), v2->bounds().ToString());
}

TEST_F(BoxLayoutTest, MainAxisAlignmentVertical) {
  layout_.reset(new BoxLayout(BoxLayout::kVertical, 10, 10, 10));

  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);

  host_->SetBounds(0, 0, 40, 100);

  // Align children to the vertical start by default.
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 40, 20, 10).ToString(), v2->bounds().ToString());

  // Ensure same results for MAIN_AXIS_ALIGNMENT_START.
  layout_->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 40, 20, 10).ToString(), v2->bounds().ToString());

  // Aligns children to the center vertically.
  layout_->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(10, 30, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 60, 20, 10).ToString(), v2->bounds().ToString());

  // Aligns children to the end of the host vertically, accounting for the
  // inside border spacing.
  layout_->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_END);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(10, 50, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 80, 20, 10).ToString(), v2->bounds().ToString());
}

}  // namespace views
