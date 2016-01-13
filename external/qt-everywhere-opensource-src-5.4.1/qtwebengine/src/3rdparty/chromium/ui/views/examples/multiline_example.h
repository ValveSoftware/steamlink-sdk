// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_MULTILINE_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_MULTILINE_EXAMPLE_H_

#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/examples/example_base.h"

namespace views {

class Checkbox;
class Label;

namespace examples {

// An example that compares the multiline rendering of different controls.
class VIEWS_EXAMPLES_EXPORT MultilineExample : public ExampleBase,
                                               public TextfieldController,
                                               public ButtonListener {
 public:
  MultilineExample();
  virtual ~MultilineExample();

  // ExampleBase:
  virtual void CreateExampleView(View* container) OVERRIDE;

  // ButtonListener:
  virtual void ButtonPressed(Button* sender, const ui::Event& event) OVERRIDE;

 private:
  class RenderTextView;

  // TextfieldController:
  virtual void ContentsChanged(Textfield* sender,
                               const base::string16& new_contents) OVERRIDE;
  virtual bool HandleKeyEvent(Textfield* sender,
                              const ui::KeyEvent& key_event) OVERRIDE;

  RenderTextView* render_text_view_;
  Label* label_;
  Textfield* textfield_;

  // Checkbox to enable and disable text rendering in |label_|.
  Checkbox* label_checkbox_;

  DISALLOW_COPY_AND_ASSIGN(MultilineExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_MULTILINE_EXAMPLE_H_
