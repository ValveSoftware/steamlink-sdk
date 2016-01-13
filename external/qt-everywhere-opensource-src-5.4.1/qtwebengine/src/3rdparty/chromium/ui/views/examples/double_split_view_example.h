// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_DOUBLE_SPLIT_VIEW_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_DOUBLE_SPLIT_VIEW_EXAMPLE_H_

#include "base/macros.h"
#include "ui/views/examples/example_base.h"

namespace views {
class SingleSplitView;

namespace examples {

class VIEWS_EXAMPLES_EXPORT DoubleSplitViewExample : public ExampleBase {
 public:
  DoubleSplitViewExample();
  virtual ~DoubleSplitViewExample();

  // ExampleBase:
  virtual void CreateExampleView(View* container) OVERRIDE;

 private:
  // The SingleSplitViews to be embedded.
  SingleSplitView* outer_single_split_view_;
  SingleSplitView* inner_single_split_view_;

  DISALLOW_COPY_AND_ASSIGN(DoubleSplitViewExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_DOUBLE_SPLIT_VIEW_EXAMPLE_H_
