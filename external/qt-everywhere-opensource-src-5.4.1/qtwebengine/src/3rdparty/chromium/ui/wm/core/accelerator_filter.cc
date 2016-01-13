// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/accelerator_filter.h"

#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"
#include "ui/wm/core/accelerator_delegate.h"

namespace wm {
namespace {

// Returns true if |key_code| is a key usually handled directly by the shell.
bool IsSystemKey(ui::KeyboardCode key_code) {
#if defined(OS_CHROMEOS)
  switch (key_code) {
    case ui::VKEY_MEDIA_LAUNCH_APP2:  // Fullscreen button.
    case ui::VKEY_MEDIA_LAUNCH_APP1:  // Overview button.
    case ui::VKEY_BRIGHTNESS_DOWN:
    case ui::VKEY_BRIGHTNESS_UP:
    case ui::VKEY_KBD_BRIGHTNESS_DOWN:
    case ui::VKEY_KBD_BRIGHTNESS_UP:
    case ui::VKEY_VOLUME_MUTE:
    case ui::VKEY_VOLUME_DOWN:
    case ui::VKEY_VOLUME_UP:
      return true;
    default:
      return false;
  }
#endif  // defined(OS_CHROMEOS)
  return false;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AcceleratorFilter, public:

AcceleratorFilter::AcceleratorFilter(scoped_ptr<AcceleratorDelegate> delegate)
    : delegate_(delegate.Pass()) {
}

AcceleratorFilter::~AcceleratorFilter() {
}

////////////////////////////////////////////////////////////////////////////////
// AcceleratorFilter, EventFilter implementation:

void AcceleratorFilter::OnKeyEvent(ui::KeyEvent* event) {
  const ui::EventType type = event->type();
  DCHECK(event->target());
  if ((type != ui::ET_KEY_PRESSED && type != ui::ET_KEY_RELEASED) ||
      event->is_char() || !event->target()) {
    return;
  }

  ui::Accelerator accelerator = CreateAcceleratorFromKeyEvent(*event);

  AcceleratorDelegate::KeyType key_type =
      IsSystemKey(event->key_code()) ? AcceleratorDelegate::KEY_TYPE_SYSTEM
                                     : AcceleratorDelegate::KEY_TYPE_OTHER;

  if (delegate_->ProcessAccelerator(*event, accelerator, key_type))
    event->StopPropagation();
}

ui::Accelerator CreateAcceleratorFromKeyEvent(const ui::KeyEvent& key_event) {
  const int kModifierFlagMask =
      (ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);

  ui::Accelerator accelerator(key_event.key_code(),
                              key_event.flags() & kModifierFlagMask);
  if (key_event.type() == ui::ET_KEY_RELEASED)
    accelerator.set_type(ui::ET_KEY_RELEASED);
  accelerator.set_is_repeat(key_event.IsRepeat());
  return accelerator;
}

}  // namespace wm
