// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/nested_accelerator_dispatcher.h"

#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/wm/core/accelerator_filter.h"
#include "ui/wm/core/nested_accelerator_delegate.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#endif

namespace wm {

namespace {

#if defined(USE_OZONE)
bool IsKeyEvent(const base::NativeEvent& native_event) {
  const ui::KeyEvent* event = static_cast<const ui::KeyEvent*>(native_event);
  return event->IsKeyEvent();
}
#elif defined(USE_X11)
bool IsKeyEvent(const XEvent* xev) {
  return xev->type == KeyPress || xev->type == KeyRelease;
}
#else
#error Unknown build platform: you should have either use_ozone or use_x11.
#endif

scoped_ptr<ui::ScopedEventDispatcher> OverrideDispatcher(
    ui::PlatformEventDispatcher* dispatcher) {
  ui::PlatformEventSource* source = ui::PlatformEventSource::GetInstance();
  return source ? source->OverrideDispatcher(dispatcher)
                : scoped_ptr<ui::ScopedEventDispatcher>();
}

}  // namespace

class NestedAcceleratorDispatcherLinux : public NestedAcceleratorDispatcher,
                                         public ui::PlatformEventDispatcher {
 public:
  explicit NestedAcceleratorDispatcherLinux(NestedAcceleratorDelegate* delegate)
      : NestedAcceleratorDispatcher(delegate),
        restore_dispatcher_(OverrideDispatcher(this)) {}

  virtual ~NestedAcceleratorDispatcherLinux() {}

 private:
  // AcceleratorDispatcher:
  virtual scoped_ptr<base::RunLoop> CreateRunLoop() OVERRIDE {
    return scoped_ptr<base::RunLoop>(new base::RunLoop());
  }

  // ui::PlatformEventDispatcher:
  virtual bool CanDispatchEvent(const ui::PlatformEvent& event) OVERRIDE {
    return true;
  }

  virtual uint32_t DispatchEvent(const ui::PlatformEvent& event) OVERRIDE {
    if (IsKeyEvent(event)) {
      ui::KeyEvent key_event(event, false);
      ui::Accelerator accelerator = CreateAcceleratorFromKeyEvent(key_event);

      switch (delegate_->ProcessAccelerator(accelerator)) {
        case NestedAcceleratorDelegate::RESULT_PROCESS_LATER:
#if defined(USE_X11)
          XPutBackEvent(event->xany.display, event);
#else
          NOTIMPLEMENTED();
#endif
          return ui::POST_DISPATCH_NONE;
        case NestedAcceleratorDelegate::RESULT_PROCESSED:
          return ui::POST_DISPATCH_NONE;
        case NestedAcceleratorDelegate::RESULT_NOT_PROCESSED:
          break;
      }
    }
    ui::PlatformEventDispatcher* prev = *restore_dispatcher_;

    return prev ? prev->DispatchEvent(event)
                : ui::POST_DISPATCH_PERFORM_DEFAULT;
  }

  scoped_ptr<ui::ScopedEventDispatcher> restore_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(NestedAcceleratorDispatcherLinux);
};

scoped_ptr<NestedAcceleratorDispatcher> NestedAcceleratorDispatcher::Create(
    NestedAcceleratorDelegate* delegate,
    base::MessagePumpDispatcher* nested_dispatcher) {
  return scoped_ptr<NestedAcceleratorDispatcher>(
      new NestedAcceleratorDispatcherLinux(delegate));
}

}  // namespace wm
