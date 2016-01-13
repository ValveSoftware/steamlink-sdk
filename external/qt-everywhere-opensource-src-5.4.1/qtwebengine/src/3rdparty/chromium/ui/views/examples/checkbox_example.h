// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_CHECKBOX_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_CHECKBOX_EXAMPLE_H_

#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/examples/example_base.h"

namespace views {
class Checkbox;

namespace examples {

// CheckboxExample exercises a Checkbox control.
class VIEWS_EXAMPLES_EXPORT CheckboxExample : public ExampleBase,
                                              public ButtonListener {
 public:
  CheckboxExample();
  virtual ~CheckboxExample();

  // ExampleBase:
  virtual void CreateExampleView(View* container) OVERRIDE;

 private:
  // ButtonListener:
  virtual void ButtonPressed(Button* sender, const ui::Event& event) OVERRIDE;

  // The only control in this test.
  Checkbox* button_;

  int count_;

  DISALLOW_COPY_AND_ASSIGN(CheckboxExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_CHECKBOX_EXAMPLE_H_
