// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/progress_bar_example.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace {

const double kStepSize = 0.1;

double SetToMax(double percent) {
  return std::min(std::max(percent, 0.0), 1.0);
}

}  // namespace

namespace views {
namespace examples {

ProgressBarExample::ProgressBarExample()
    : ExampleBase("Progress Bar"),
      minus_button_(NULL),
      plus_button_(NULL),
      progress_bar_(NULL),
      current_percent_(0.0) {
}

ProgressBarExample::~ProgressBarExample() {
}

void ProgressBarExample::CreateExampleView(View* container) {
  GridLayout* layout = new GridLayout(container);
  container->SetLayoutManager(layout);

  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL,
                        0, GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, 8);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL,
                        1, GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, 8);
  column_set->AddColumn(GridLayout::TRAILING, GridLayout::FILL,
                        0, GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  minus_button_ = new LabelButton(this, base::ASCIIToUTF16("-"));
  layout->AddView(minus_button_);
  progress_bar_ = new ProgressBar();
  layout->AddView(progress_bar_);
  plus_button_ = new LabelButton(this, base::ASCIIToUTF16("+"));
  layout->AddView(plus_button_);
}

void ProgressBarExample::ButtonPressed(Button* sender, const ui::Event& event) {
  if (sender == minus_button_)
    current_percent_ = SetToMax(current_percent_ - kStepSize);
  else if (sender == plus_button_)
    current_percent_ = SetToMax(current_percent_ + kStepSize);

  progress_bar_->SetValue(current_percent_);
}

}  // namespace examples
}  // namespace views
