// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_ACCELERATOR_FILTER_H_
#define UI_WM_CORE_ACCELERATOR_FILTER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/events/event_handler.h"
#include "ui/wm/wm_export.h"

namespace ui {
class Accelerator;
}

namespace wm {
class AcceleratorDelegate;

// AcceleratorFilter filters key events for AcceleratorControler handling global
// keyboard accelerators.
class WM_EXPORT AcceleratorFilter : public ui::EventHandler {
 public:
  AcceleratorFilter(scoped_ptr<AcceleratorDelegate> delegate);
  virtual ~AcceleratorFilter();

  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;

 private:
  scoped_ptr<AcceleratorDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorFilter);
};

ui::Accelerator CreateAcceleratorFromKeyEvent(const ui::KeyEvent& key_event);

}  // namespace wm

#endif  // UI_WM_CORE_ACCELERATOR_FILTER_H_
