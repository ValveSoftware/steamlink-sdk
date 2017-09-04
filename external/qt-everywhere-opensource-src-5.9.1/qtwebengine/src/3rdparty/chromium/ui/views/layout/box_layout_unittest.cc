// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/box_layout.h"

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/test_views.h"
#include "ui/views/view.h"

namespace views {

namespace {

class BoxLayoutTest : public testing::Test {
 public:
  void SetUp() override { host_.reset(new View); }

  std::unique_ptr<View> host_;
};

}  // namespace

TEST_F(BoxLayoutTest, Empty) {
  BoxLayout* layout = new BoxLayout(BoxLayout::kHorizontal, 10, 10, 20);
  host_->SetLayoutManager(layout);
  EXPECT_EQ(gfx::Size(20, 20), layout->GetPreferredSize(host_.get()));
}

TEST_F(BoxLayoutTest, AlignmentHorizontal) {
  BoxLayout* layout = new BoxLayout(BoxLayout::kHorizontal, 0, 0, 0);
  host_->SetLayoutManager(layout);
  View* v1 = new StaticSizedView(gfx::Size(10, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(20, 20), layout->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 20, 20);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 10, 20), v1->bounds());
  EXPECT_EQ(gfx::Rect(10, 0, 10, 20), v2->bounds());
}

TEST_F(BoxLayoutTest, AlignmentVertical) {
  BoxLayout* layout = new BoxLayout(BoxLayout::kVertical, 0, 0, 0);
  host_->SetLayoutManager(layout);
  View* v1 = new StaticSizedView(gfx::Size(20, 10));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(20, 20), layout->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 20, 20);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 20, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(0, 10, 20, 10), v2->bounds());
}

TEST_F(BoxLayoutTest, SetInsideBorderInsets) {
  BoxLayout* layout = new BoxLayout(BoxLayout::kHorizontal, 10, 20, 0);
  host_->SetLayoutManager(layout);
  View* v1 = new StaticSizedView(gfx::Size(10, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(40, 60), layout->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 40, 60);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 20, 10, 20), v1->bounds());
  EXPECT_EQ(gfx::Rect(20, 20, 10, 20), v2->bounds());

  layout->set_inside_border_insets(
      gfx::Insets(5, 10, 15, 20));
  EXPECT_EQ(gfx::Size(50, 40), layout->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 50, 40);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 5, 10, 20), v1->bounds());
  EXPECT_EQ(gfx::Rect(20, 5, 10, 20), v2->bounds());
}

TEST_F(BoxLayoutTest, Spacing) {
  BoxLayout* layout = new BoxLayout(BoxLayout::kHorizontal, 7, 7, 8);
  host_->SetLayoutManager(layout);
  View* v1 = new StaticSizedView(gfx::Size(10, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 20));
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(42, 34), layout->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 100, 100);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(7, 7, 10, 86), v1->bounds());
  EXPECT_EQ(gfx::Rect(25, 7, 10, 86), v2->bounds());
}

TEST_F(BoxLayoutTest, Overflow) {
  BoxLayout* layout = new BoxLayout(BoxLayout::kHorizontal, 0, 0, 0);
  host_->SetLayoutManager(layout);
  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 20));
  host_->AddChildView(v2);
  host_->SetBounds(0, 0, 10, 10);

  // Overflows by positioning views at the start and truncating anything that
  // doesn't fit.
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), v2->bounds());

  // All values of main axis alignment should overflow in the same manner.
  layout->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), v2->bounds());

  layout->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), v2->bounds());

  layout->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_END);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), v2->bounds());
}

TEST_F(BoxLayoutTest, NoSpace) {
  BoxLayout* layout = new BoxLayout(BoxLayout::kHorizontal, 10, 10, 10);
  host_->SetLayoutManager(layout);
  View* childView = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(childView);
  host_->SetBounds(0, 0, 10, 10);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), childView->bounds());
}

TEST_F(BoxLayoutTest, InvisibleChild) {
  BoxLayout* layout = new BoxLayout(BoxLayout::kHorizontal, 10, 10, 10);
  host_->SetLayoutManager(layout);
  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  v1->SetVisible(false);
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(30, 30), layout->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 30, 30);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 10, 10), v2->bounds());
}

TEST_F(BoxLayoutTest, UseHeightForWidth) {
  BoxLayout* layout = new BoxLayout(BoxLayout::kVertical, 0, 0, 0);
  host_->SetLayoutManager(layout);
  View* v1 = new StaticSizedView(gfx::Size(20, 10));
  host_->AddChildView(v1);
  ProportionallySizedView* v2 = new ProportionallySizedView(2);
  v2->SetPreferredWidth(10);
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(20, 50), layout->GetPreferredSize(host_.get()));

  host_->SetBounds(0, 0, 20, 50);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 20, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(0, 10, 20, 40), v2->bounds());

  EXPECT_EQ(110, layout->GetPreferredHeightForWidth(host_.get(), 50));

  // Test without horizontal stretching of the views.
  layout->set_cross_axis_alignment(BoxLayout::CROSS_AXIS_ALIGNMENT_END);
  EXPECT_EQ(gfx::Size(20, 30).ToString(),
            layout->GetPreferredSize(host_.get()).ToString());

  host_->SetBounds(0, 0, 20, 30);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(0, 0, 20, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(10, 10, 10, 20), v2->bounds());

  EXPECT_EQ(30, layout->GetPreferredHeightForWidth(host_.get(), 50));
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

// Verifies that a BoxLayout correctly handles child spacing, flex layout, and
// empty preferred size, simultaneously.
TEST_F(BoxLayoutTest, EmptyPreferredSizeWithFlexLayoutAndChildSpacing) {
  host_->RemoveAllChildViews(true);
  BoxLayout* layout = new BoxLayout(BoxLayout::kHorizontal, 0, 0, 5);
  host_->SetLayoutManager(layout);
  View* v1 = new StaticSizedView(gfx::Size());
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  View* v3 = new StaticSizedView(gfx::Size(1, 1));
  host_->AddChildViewAt(v3, 0);
  layout->SetFlexForView(v3, 1);
  host_->SetSize(gfx::Size(1000, 15));
  host_->Layout();

  EXPECT_EQ(host_->bounds().right(), v2->bounds().right());
}

TEST_F(BoxLayoutTest, MainAxisAlignmentHorizontal) {
  BoxLayout* layout = new BoxLayout(BoxLayout::kHorizontal, 10, 10, 10);
  host_->SetLayoutManager(layout);

  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);

  host_->SetBounds(0, 0, 100, 40);

  // Align children to the horizontal start by default.
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(40, 10, 10, 20).ToString(), v2->bounds().ToString());

  // Ensure same results for MAIN_AXIS_ALIGNMENT_START.
  layout->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(40, 10, 10, 20).ToString(), v2->bounds().ToString());

  // Aligns children to the center horizontally.
  layout->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(30, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(60, 10, 10, 20).ToString(), v2->bounds().ToString());

  // Aligns children to the end of the host horizontally, accounting for the
  // inside border spacing.
  layout->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_END);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(50, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(80, 10, 10, 20).ToString(), v2->bounds().ToString());
}

TEST_F(BoxLayoutTest, MainAxisAlignmentVertical) {
  BoxLayout* layout = new BoxLayout(BoxLayout::kVertical, 10, 10, 10);
  host_->SetLayoutManager(layout);

  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);

  host_->SetBounds(0, 0, 40, 100);

  // Align children to the vertical start by default.
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 40, 20, 10).ToString(), v2->bounds().ToString());

  // Ensure same results for MAIN_AXIS_ALIGNMENT_START.
  layout->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 40, 20, 10).ToString(), v2->bounds().ToString());

  // Aligns children to the center vertically.
  layout->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 30, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 60, 20, 10).ToString(), v2->bounds().ToString());

  // Aligns children to the end of the host vertically, accounting for the
  // inside border spacing.
  layout->set_main_axis_alignment(BoxLayout::MAIN_AXIS_ALIGNMENT_END);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 50, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 80, 20, 10).ToString(), v2->bounds().ToString());
}

TEST_F(BoxLayoutTest, CrossAxisAlignmentHorizontal) {
  BoxLayout* layout = new BoxLayout(BoxLayout::kHorizontal, 10, 10, 10);
  host_->SetLayoutManager(layout);

  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);

  host_->SetBounds(0, 0, 100, 60);

  // Stretch children to fill the available height by default.
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 20, 40).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(40, 10, 10, 40).ToString(), v2->bounds().ToString());

  // Ensure same results for CROSS_AXIS_ALIGNMENT_STRETCH.
  layout->set_cross_axis_alignment(BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 20, 40).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(40, 10, 10, 40).ToString(), v2->bounds().ToString());

  // Aligns children to the start vertically.
  layout->set_cross_axis_alignment(BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(40, 10, 10, 10).ToString(), v2->bounds().ToString());

  // Aligns children to the center vertically.
  layout->set_cross_axis_alignment(BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 20, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(40, 25, 10, 10).ToString(), v2->bounds().ToString());

  // Aligns children to the end of the host vertically, accounting for the
  // inside border spacing.
  layout->set_cross_axis_alignment(BoxLayout::CROSS_AXIS_ALIGNMENT_END);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 30, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(40, 40, 10, 10).ToString(), v2->bounds().ToString());
}

TEST_F(BoxLayoutTest, CrossAxisAlignmentVertical) {
  BoxLayout* layout = new BoxLayout(BoxLayout::kVertical, 10, 10, 10);
  host_->SetLayoutManager(layout);

  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);

  host_->SetBounds(0, 0, 60, 100);

  // Stretch children to fill the available width by default.
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 40, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 40, 40, 10).ToString(), v2->bounds().ToString());

  // Ensure same results for CROSS_AXIS_ALIGNMENT_STRETCH.
  layout->set_cross_axis_alignment(BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 40, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 40, 40, 10).ToString(), v2->bounds().ToString());

  // Aligns children to the start horizontally.
  layout->set_cross_axis_alignment(BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 40, 10, 10).ToString(), v2->bounds().ToString());

  // Aligns children to the center horizontally.
  layout->set_cross_axis_alignment(BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(20, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(25, 40, 10, 10).ToString(), v2->bounds().ToString());

  // Aligns children to the end of the host horizontally, accounting for the
  // inside border spacing.
  layout->set_cross_axis_alignment(BoxLayout::CROSS_AXIS_ALIGNMENT_END);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(30, 10, 20, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(40, 40, 10, 10).ToString(), v2->bounds().ToString());
}

TEST_F(BoxLayoutTest, FlexAll) {
  BoxLayout* layout = new BoxLayout(BoxLayout::kHorizontal, 10, 10, 10);
  host_->SetLayoutManager(layout);
  layout->SetDefaultFlex(1);

  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  View* v3 = new StaticSizedView(gfx::Size(30, 30));
  host_->AddChildView(v3);
  EXPECT_EQ(gfx::Size(100, 50), layout->GetPreferredSize(host_.get()));

  host_->SetBounds(0, 0, 120, 50);
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 27, 30).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(47, 10, 16, 30).ToString(), v2->bounds().ToString());
  EXPECT_EQ(gfx::Rect(73, 10, 37, 30).ToString(), v3->bounds().ToString());
}

TEST_F(BoxLayoutTest, FlexGrowVertical) {
  BoxLayout* layout = new BoxLayout(BoxLayout::kVertical, 10, 10, 10);
  host_->SetLayoutManager(layout);

  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  View* v3 = new StaticSizedView(gfx::Size(30, 30));
  host_->AddChildView(v3);

  host_->SetBounds(0, 0, 50, 130);

  // Views don't fill the available space by default.
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 30, 20).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 40, 30, 10).ToString(), v2->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 60, 30, 30).ToString(), v3->bounds().ToString());

  std::vector<BoxLayout::MainAxisAlignment> main_alignments;
  main_alignments.push_back(BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  main_alignments.push_back(BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  main_alignments.push_back(BoxLayout::MAIN_AXIS_ALIGNMENT_END);

  for (size_t i = 0; i < main_alignments.size(); ++i) {
    layout->set_main_axis_alignment(main_alignments[i]);

    // Set the first view to consume all free space.
    layout->SetFlexForView(v1, 1);
    layout->ClearFlexForView(v2);
    layout->ClearFlexForView(v3);
    host_->Layout();
    EXPECT_EQ(gfx::Rect(10, 10, 30, 50).ToString(), v1->bounds().ToString());
    EXPECT_EQ(gfx::Rect(10, 70, 30, 10).ToString(), v2->bounds().ToString());
    EXPECT_EQ(gfx::Rect(10, 90, 30, 30).ToString(), v3->bounds().ToString());

    // Set the third view to take 2/3s of the free space and leave the first
    // view
    // with 1/3.
    layout->SetFlexForView(v3, 2);
    host_->Layout();
    EXPECT_EQ(gfx::Rect(10, 10, 30, 30).ToString(), v1->bounds().ToString());
    EXPECT_EQ(gfx::Rect(10, 50, 30, 10).ToString(), v2->bounds().ToString());
    EXPECT_EQ(gfx::Rect(10, 70, 30, 50).ToString(), v3->bounds().ToString());

    // Clear the previously set flex values and set the second view to take all
    // the free space.
    layout->ClearFlexForView(v1);
    layout->SetFlexForView(v2, 1);
    layout->ClearFlexForView(v3);
    host_->Layout();
    EXPECT_EQ(gfx::Rect(10, 10, 30, 20).ToString(), v1->bounds().ToString());
    EXPECT_EQ(gfx::Rect(10, 40, 30, 40).ToString(), v2->bounds().ToString());
    EXPECT_EQ(gfx::Rect(10, 90, 30, 30).ToString(), v3->bounds().ToString());
  }
}

TEST_F(BoxLayoutTest, FlexGrowHorizontalWithRemainder) {
  BoxLayout* layout = new BoxLayout(BoxLayout::kHorizontal, 0, 0, 0);
  host_->SetLayoutManager(layout);
  layout->SetDefaultFlex(1);
  std::vector<View*> views;
  for (int i = 0; i < 5; ++i) {
    View* view = new StaticSizedView(gfx::Size(10, 10));
    views.push_back(view);
    host_->AddChildView(view);
  }

  EXPECT_EQ(gfx::Size(50, 10), layout->GetPreferredSize(host_.get()));

  host_->SetBounds(0, 0, 52, 10);
  host_->Layout();
  // The 2nd and 4th views should have an extra pixel as they correspond to 20.8
  // and 41.6 which round up.
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10).ToString(), views[0]->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 0, 11, 10).ToString(), views[1]->bounds().ToString());
  EXPECT_EQ(gfx::Rect(21, 0, 10, 10).ToString(), views[2]->bounds().ToString());
  EXPECT_EQ(gfx::Rect(31, 0, 11, 10).ToString(), views[3]->bounds().ToString());
  EXPECT_EQ(gfx::Rect(42, 0, 10, 10).ToString(), views[4]->bounds().ToString());
}

TEST_F(BoxLayoutTest, FlexGrowHorizontalWithRemainder2) {
  BoxLayout* layout = new BoxLayout(BoxLayout::kHorizontal, 0, 0, 0);
  host_->SetLayoutManager(layout);
  layout->SetDefaultFlex(1);
  std::vector<View*> views;
  for (int i = 0; i < 4; ++i) {
    View* view = new StaticSizedView(gfx::Size(1, 10));
    views.push_back(view);
    host_->AddChildView(view);
  }

  EXPECT_EQ(gfx::Size(4, 10), layout->GetPreferredSize(host_.get()));

  host_->SetBounds(0, 0, 10, 10);
  host_->Layout();
  // The 1st and 3rd views should have an extra pixel as they correspond to 2.5
  // and 7.5 which round up.
  EXPECT_EQ(gfx::Rect(0, 0, 3, 10).ToString(), views[0]->bounds().ToString());
  EXPECT_EQ(gfx::Rect(3, 0, 2, 10).ToString(), views[1]->bounds().ToString());
  EXPECT_EQ(gfx::Rect(5, 0, 3, 10).ToString(), views[2]->bounds().ToString());
  EXPECT_EQ(gfx::Rect(8, 0, 2, 10).ToString(), views[3]->bounds().ToString());
}

TEST_F(BoxLayoutTest, FlexShrinkHorizontal) {
  BoxLayout* layout = new BoxLayout(BoxLayout::kHorizontal, 10, 10, 10);
  host_->SetLayoutManager(layout);

  View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  View* v3 = new StaticSizedView(gfx::Size(30, 30));
  host_->AddChildView(v3);

  host_->SetBounds(0, 0, 85, 50);

  // Truncate width by default.
  host_->Layout();
  EXPECT_EQ(gfx::Rect(10, 10, 20, 30).ToString(), v1->bounds().ToString());
  EXPECT_EQ(gfx::Rect(40, 10, 10, 30).ToString(), v2->bounds().ToString());
  EXPECT_EQ(gfx::Rect(60, 10, 15, 30).ToString(), v3->bounds().ToString());

  std::vector<BoxLayout::MainAxisAlignment> main_alignments;
  main_alignments.push_back(BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  main_alignments.push_back(BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  main_alignments.push_back(BoxLayout::MAIN_AXIS_ALIGNMENT_END);

  for (size_t i = 0; i < main_alignments.size(); ++i) {
    layout->set_main_axis_alignment(main_alignments[i]);

    // Set the first view to shrink as much as necessary.
    layout->SetFlexForView(v1, 1);
    layout->ClearFlexForView(v2);
    layout->ClearFlexForView(v3);
    host_->Layout();
    EXPECT_EQ(gfx::Rect(10, 10, 5, 30).ToString(), v1->bounds().ToString());
    EXPECT_EQ(gfx::Rect(25, 10, 10, 30).ToString(), v2->bounds().ToString());
    EXPECT_EQ(gfx::Rect(45, 10, 30, 30).ToString(), v3->bounds().ToString());

    // Set the third view to shrink 2/3s of the free space and leave the first
    // view with 1/3.
    layout->SetFlexForView(v3, 2);
    host_->Layout();
    EXPECT_EQ(gfx::Rect(10, 10, 15, 30).ToString(), v1->bounds().ToString());
    EXPECT_EQ(gfx::Rect(35, 10, 10, 30).ToString(), v2->bounds().ToString());
    EXPECT_EQ(gfx::Rect(55, 10, 20, 30).ToString(), v3->bounds().ToString());

    // Clear the previously set flex values and set the second view to take all
    // the free space with MAIN_AXIS_ALIGNMENT_END set. This causes the second
    // view to shrink to zero and the third view still doesn't fit so it
    // overflows.
    layout->ClearFlexForView(v1);
    layout->SetFlexForView(v2, 2);
    layout->ClearFlexForView(v3);
    host_->Layout();
    EXPECT_EQ(gfx::Rect(10, 10, 20, 30).ToString(), v1->bounds().ToString());
    // Conceptually this view is at 10, 40, 0, 0.
    EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(), v2->bounds().ToString());
    EXPECT_EQ(gfx::Rect(50, 10, 25, 30).ToString(), v3->bounds().ToString());
  }
}

TEST_F(BoxLayoutTest, FlexShrinkVerticalWithRemainder) {
  BoxLayout* layout = new BoxLayout(BoxLayout::kVertical, 0, 0, 0);
  host_->SetLayoutManager(layout);
  View* v1 = new StaticSizedView(gfx::Size(20, 10));
  host_->AddChildView(v1);
  View* v2 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v2);
  View* v3 = new StaticSizedView(gfx::Size(20, 10));
  host_->AddChildView(v3);
  host_->SetBounds(0, 0, 20, 20);

  std::vector<BoxLayout::MainAxisAlignment> main_alignments;
  main_alignments.push_back(BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  main_alignments.push_back(BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  main_alignments.push_back(BoxLayout::MAIN_AXIS_ALIGNMENT_END);

  for (size_t i = 0; i < main_alignments.size(); ++i) {
    layout->set_main_axis_alignment(main_alignments[i]);

    // The first view shrinks by 1/3 of the excess, the second view shrinks by
    // 2/3 of the excess and the third view should maintain its preferred size.
    layout->SetFlexForView(v1, 1);
    layout->SetFlexForView(v2, 2);
    layout->ClearFlexForView(v3);
    host_->Layout();
    EXPECT_EQ(gfx::Rect(0, 0, 20, 3).ToString(), v1->bounds().ToString());
    EXPECT_EQ(gfx::Rect(0, 3, 20, 7).ToString(), v2->bounds().ToString());
    EXPECT_EQ(gfx::Rect(0, 10, 20, 10).ToString(), v3->bounds().ToString());

    // The second view shrinks to 2/3 of the excess, the third view shrinks to
    // 1/3 of the excess and the first view should maintain its preferred size.
    layout->ClearFlexForView(v1);
    layout->SetFlexForView(v2, 2);
    layout->SetFlexForView(v3, 1);
    host_->Layout();
    EXPECT_EQ(gfx::Rect(0, 0, 20, 10).ToString(), v1->bounds().ToString());
    EXPECT_EQ(gfx::Rect(0, 10, 20, 7).ToString(), v2->bounds().ToString());
    EXPECT_EQ(gfx::Rect(0, 17, 20, 3).ToString(), v3->bounds().ToString());

    // Each view shrinks equally to fit within the available space.
    layout->SetFlexForView(v1, 1);
    layout->SetFlexForView(v2, 1);
    layout->SetFlexForView(v3, 1);
    host_->Layout();
    EXPECT_EQ(gfx::Rect(0, 0, 20, 3).ToString(), v1->bounds().ToString());
    EXPECT_EQ(gfx::Rect(0, 3, 20, 14).ToString(), v2->bounds().ToString());
    EXPECT_EQ(gfx::Rect(0, 17, 20, 3).ToString(), v3->bounds().ToString());
  }
}

TEST_F(BoxLayoutTest, MinimumCrossAxisVertical) {
  BoxLayout* layout = new BoxLayout(BoxLayout::kVertical, 0, 0, 0);
  host_->SetLayoutManager(layout);
  View* v1 = new StaticSizedView(gfx::Size(20, 10));
  host_->AddChildView(v1);
  layout->set_minimum_cross_axis_size(30);

  EXPECT_EQ(gfx::Size(30, 10), layout->GetPreferredSize(host_.get()));
}

TEST_F(BoxLayoutTest, MinimumCrossAxisHorizontal) {
  BoxLayout* layout = new BoxLayout(BoxLayout::kHorizontal, 0, 0, 0);
  host_->SetLayoutManager(layout);
  View* v1 = new StaticSizedView(gfx::Size(20, 10));
  host_->AddChildView(v1);
  layout->set_minimum_cross_axis_size(30);

  EXPECT_EQ(gfx::Size(20, 30), layout->GetPreferredSize(host_.get()));
}

}  // namespace views
