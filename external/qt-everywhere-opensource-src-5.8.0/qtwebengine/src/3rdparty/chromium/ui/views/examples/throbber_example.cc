// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/throbber_example.h"

#include "base/macros.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace views {
namespace examples {

namespace {

class ThrobberView : public View {
 public:
  ThrobberView() : throbber_(new Throbber()) {
    AddChildView(throbber_);
    throbber_->Start();
  }

  gfx::Size GetPreferredSize() const override {
    return gfx::Size(width(), height());
  }

  void Layout() override {
    int diameter = 64;
    throbber_->SetBounds((width() - diameter) / 2,
                         (height() - diameter) / 2,
                         diameter, diameter);
    SizeToPreferredSize();
  }

 private:
  Throbber* throbber_;

  DISALLOW_COPY_AND_ASSIGN(ThrobberView);
};

}  // namespace

ThrobberExample::ThrobberExample() : ExampleBase("Throbber") {
}

ThrobberExample::~ThrobberExample() {
}

void ThrobberExample::CreateExampleView(View* container) {
  container->SetLayoutManager(new FillLayout());
  container->AddChildView(new ThrobberView());
}

}  // namespace examples
}  // namespace views
