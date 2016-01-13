// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/throbber_example.h"

#include "ui/views/controls/throbber.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace views {
namespace examples {

namespace {

// Time in ms per throbber frame.
const int kThrobberFrameMs = 60;

class ThrobberView : public View {
 public:
  ThrobberView() {
    throbber_ = new Throbber(kThrobberFrameMs, false);
    AddChildView(throbber_);
    throbber_->SetVisible(true);
    throbber_->Start();
  }

  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    return gfx::Size(width(), height());
  }

  virtual void Layout() OVERRIDE {
    View* child = child_at(0);
    gfx::Size ps = child->GetPreferredSize();
    child->SetBounds((width() - ps.width()) / 2,
                     (height() - ps.height()) / 2,
                     ps.width(), ps.height());
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
