// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_LISTENER_H_
#define UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_LISTENER_H_

#include "ui/views/views_export.h"

namespace gfx {
class Point;
}

namespace views {

class View;

// An interface implemented by an object to let it know that a menu button was
// clicked.
class VIEWS_EXPORT MenuButtonListener {
 public:
  virtual void OnMenuButtonClicked(View* source, const gfx::Point& point) = 0;

 protected:
  virtual ~MenuButtonListener() {}
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_LISTENER_H_
