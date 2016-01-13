// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_INPUT_METHOD_EVENT_FILTER_H_
#define UI_WM_CORE_INPUT_METHOD_EVENT_FILTER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/wm/wm_export.h"

namespace ui {
class EventProcessor;
class InputMethod;
}

namespace wm {

// An event filter that forwards a KeyEvent to a system IME, and dispatches a
// TranslatedKeyEvent to the root window as needed.
class WM_EXPORT InputMethodEventFilter
    : public ui::EventHandler,
      public ui::internal::InputMethodDelegate {
 public:
  explicit InputMethodEventFilter(gfx::AcceleratedWidget widget);
  virtual ~InputMethodEventFilter();

  void SetInputMethodPropertyInRootWindow(aura::Window* root_window);

  ui::InputMethod* input_method() const { return input_method_.get(); }

 private:
  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;

  // Overridden from ui::internal::InputMethodDelegate:
  virtual bool DispatchKeyEventPostIME(const ui::KeyEvent& event) OVERRIDE;

  scoped_ptr<ui::InputMethod> input_method_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodEventFilter);
};

}  // namespace wm

#endif  // UI_WM_CORE_INPUT_METHOD_EVENT_FILTER_H_
