// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_NESTED_ACCELERATOR_DISPATCHER_H_
#define UI_WM_CORE_NESTED_ACCELERATOR_DISPATCHER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/wm/wm_export.h"

namespace base {
class MessagePumpDispatcher;
class RunLoop;
}

namespace ui {
class KeyEvent;
}

namespace wm {

class NestedAcceleratorDelegate;

// Dispatcher for handling accelerators from menu.
//
// Wraps a nested dispatcher to which control is passed if no accelerator key
// has been pressed. If the nested dispatcher is NULL, then the control is
// passed back to the default dispatcher.
// TODO(pkotwicz): Add support for a |nested_dispatcher| which sends
//  events to a system IME.
class WM_EXPORT NestedAcceleratorDispatcher {
 public:
  virtual ~NestedAcceleratorDispatcher();

  static scoped_ptr<NestedAcceleratorDispatcher> Create(
      NestedAcceleratorDelegate* dispatcher_delegate,
      base::MessagePumpDispatcher* nested_dispatcher);

  // Creates a base::RunLoop object to run a nested message loop.
  virtual scoped_ptr<base::RunLoop> CreateRunLoop() = 0;

 protected:
  explicit NestedAcceleratorDispatcher(NestedAcceleratorDelegate* delegate);

  NestedAcceleratorDelegate*
      delegate_;  // Owned by NestedAcceleratorController.

 private:
  DISALLOW_COPY_AND_ASSIGN(NestedAcceleratorDispatcher);
};

}  // namespace wm

#endif  // UI_WM_CORE_NESTED_ACCELERATOR_DISPATCHER_H_
