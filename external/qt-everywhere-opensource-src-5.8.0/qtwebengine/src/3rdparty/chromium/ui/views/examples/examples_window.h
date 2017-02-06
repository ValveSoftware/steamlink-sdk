// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_EXAMPLES_WINDOW_H_
#define UI_VIEWS_EXAMPLES_EXAMPLES_WINDOW_H_

#include <memory>

#include "base/memory/scoped_vector.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/examples/views_examples_export.h"

namespace aura {
class Window;
}

namespace views {
namespace examples {

class ExampleBase;

enum Operation {
  DO_NOTHING_ON_CLOSE = 0,
  QUIT_ON_CLOSE,
};

// Shows a window with the views examples in it. |extra_examples| contains any
// additional examples to add. |window_context| is used to determine where the
// window should be created (see |Widget::InitParams::context| for details).
VIEWS_EXAMPLES_EXPORT void ShowExamplesWindow(
    Operation operation,
    gfx::NativeWindow window_context,
    std::unique_ptr<ScopedVector<ExampleBase>> extra_examples);

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_EXAMPLES_WINDOW_H_
