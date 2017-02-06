// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/double_split_view_example.h"

#include "base/macros.h"
#include "ui/views/background.h"
#include "ui/views/controls/single_split_view.h"
#include "ui/views/layout/grid_layout.h"

namespace views {
namespace examples {

namespace {

// DoubleSplitViews's content, which draws gradient color on background.
class SplittedView : public View {
 public:
  SplittedView();
  ~SplittedView() override;

  void SetColor(SkColor from, SkColor to);

  // View:
  gfx::Size GetMinimumSize() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SplittedView);
};

SplittedView::SplittedView() {
  SetColor(SK_ColorRED, SK_ColorGREEN);
}

SplittedView::~SplittedView() {
}

void SplittedView::SetColor(SkColor from, SkColor to) {
  set_background(Background::CreateVerticalGradientBackground(from, to));
}

gfx::Size SplittedView::GetMinimumSize() const {
  return gfx::Size(10, 10);
}

}  // namespace

DoubleSplitViewExample::DoubleSplitViewExample()
    : ExampleBase("Double Split View") {
}

DoubleSplitViewExample::~DoubleSplitViewExample() {
}

void DoubleSplitViewExample::CreateExampleView(View* container) {
  SplittedView* splitted_view_1 = new SplittedView();
  SplittedView* splitted_view_2 = new SplittedView();
  SplittedView* splitted_view_3 = new SplittedView();

  inner_single_split_view_ = new SingleSplitView(
      splitted_view_1, splitted_view_2,
      SingleSplitView::HORIZONTAL_SPLIT,
      NULL);

  outer_single_split_view_ = new SingleSplitView(
      inner_single_split_view_, splitted_view_3,
      SingleSplitView::HORIZONTAL_SPLIT,
      NULL);

  GridLayout* layout = new GridLayout(container);
  container->SetLayoutManager(layout);

  // Add scroll view.
  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRow(1, 0);
  layout->AddView(outer_single_split_view_);
}

}  // namespace examples
}  // namespace views
