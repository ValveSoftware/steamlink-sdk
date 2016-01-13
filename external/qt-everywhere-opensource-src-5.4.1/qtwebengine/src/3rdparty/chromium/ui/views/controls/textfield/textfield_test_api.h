// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_TEST_API_H_
#define UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_TEST_API_H_

#include "ui/views/controls/textfield/textfield.h"

namespace views {

// Helper class to access internal state of Textfield in tests.
class TextfieldTestApi {
 public:
  explicit TextfieldTestApi(Textfield* textfield);

  void UpdateContextMenu();

  gfx::RenderText* GetRenderText() const;

  void CreateTouchSelectionControllerAndNotifyIt();

  void ResetTouchSelectionController();

  TextfieldModel* model() const { return textfield_->model_.get(); }

  ui::MenuModel* context_menu_contents() const {
    return textfield_->context_menu_contents_.get();
  }

  ui::TouchSelectionController* touch_selection_controller() const {
    return textfield_->touch_selection_controller_.get();
  }

 private:
  Textfield* textfield_;

  DISALLOW_COPY_AND_ASSIGN(TextfieldTestApi);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_TEST_API_H_
