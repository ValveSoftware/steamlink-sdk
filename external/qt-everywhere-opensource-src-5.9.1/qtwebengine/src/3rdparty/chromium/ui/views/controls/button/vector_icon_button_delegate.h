// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_VECTOR_ICON_DELEGATE_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_VECTOR_ICON_DELEGATE_BUTTON_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/views_export.h"

namespace views {

// A ButtonListener that also provides a color to use for drawing an icon.
class VIEWS_EXPORT VectorIconButtonDelegate : public ButtonListener {
 public:
  virtual SkColor GetVectorIconBaseColor() const;

 protected:
  ~VectorIconButtonDelegate() override {}
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_VECTOR_ICON_BUTTON_DELEGATE_H_
