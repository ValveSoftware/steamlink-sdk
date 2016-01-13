// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/input_method_event_filter.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"

namespace wm {

////////////////////////////////////////////////////////////////////////////////
// InputMethodEventFilter, public:

InputMethodEventFilter::InputMethodEventFilter(gfx::AcceleratedWidget widget)
    : input_method_(ui::CreateInputMethod(this, widget)) {
  // TODO(yusukes): Check if the root window is currently focused and pass the
  // result to Init().
  input_method_->Init(true);
}

InputMethodEventFilter::~InputMethodEventFilter() {
}

void InputMethodEventFilter::SetInputMethodPropertyInRootWindow(
    aura::Window* root_window) {
  root_window->SetProperty(aura::client::kRootWindowInputMethodKey,
                           input_method_.get());
}

////////////////////////////////////////////////////////////////////////////////
// InputMethodEventFilter, EventFilter implementation:

void InputMethodEventFilter::OnKeyEvent(ui::KeyEvent* event) {
  // We're processing key events as follows (details are simplified).
  //
  // At the beginning, key events have a ET_KEY_{PRESSED,RELEASED} event type,
  // and they're passed from step 1 through step 3.
  //   1. EventProcessor::OnEventFromSource()
  //   2. InputMethodEventFilter::OnKeyEvent()
  //   3. InputMethod::DispatchKeyEvent()
  // where InputMethod may call DispatchKeyEventPostIME() if IME didn't consume
  // the key event.  Otherwise, step 4 through step 6 are skipped and we fall
  // down to step 7 directly.
  //   4. InputMethodEventFilter::DispatchKeyEventPostIME()
  // where the key event is marked as TRANSLATED and the event type becomes
  // ET_TRANSLATED_KEY_{PRESS,RELEASE}.  Then, we dispatch the event again from
  // the beginning.
  //   5. EventProcessor::OnEventFromSource()     [second time]
  //   6. InputMethodEventFilter::OnKeyEvent()    [second time]
  // where we know that the event was already processed once by IME and
  // re-dispatched, we don't pass the event to IME again.  Instead we unmark the
  // event as not translated (as same as the original state), and let the event
  // dispatcher continue to dispatch the event to the rest event handlers.
  //   7. EventHandler::OnKeyEvent()
  if (event->IsTranslated()) {
    // The |event| was already processed by IME, so we don't pass the event to
    // IME again.  Just let the event dispatcher continue to dispatch the event.
    event->SetTranslated(false);
  } else {
    if (input_method_->DispatchKeyEvent(*event))
      event->StopPropagation();
  }
}

////////////////////////////////////////////////////////////////////////////////
// InputMethodEventFilter, ui::InputMethodDelegate implementation:

bool InputMethodEventFilter::DispatchKeyEventPostIME(
    const ui::KeyEvent& event) {
#if defined(OS_WIN)
  DCHECK(!event.HasNativeEvent() || event.native_event().message != WM_CHAR);
#endif
  // Since the underlying IME didn't consume the key event, we're going to
  // dispatch the event again from the beginning of the tree of event targets.
  // This time we have to skip dispatching the event to the IME, we mark the
  // event as TRANSLATED so we can distinguish this event as a second time
  // dispatched event.
  // For the target where to dispatch the event, always tries the current
  // focused text input client's attached window. And fallback to the target
  // carried by event.
  aura::Window* target_window = NULL;
  ui::TextInputClient* input = input_method_->GetTextInputClient();
  if (input)
    target_window = input->GetAttachedWindow();
  if (!target_window)
    target_window = static_cast<aura::Window*>(event.target());
  if (!target_window)
    return false;
  ui::EventProcessor* target_dispatcher =
      target_window->GetRootWindow()->GetHost()->event_processor();
  ui::KeyEvent aura_event(event);
  aura_event.SetTranslated(true);
  ui::EventDispatchDetails details =
      target_dispatcher->OnEventFromSource(&aura_event);
  CHECK(!details.dispatcher_destroyed);
  return aura_event.handled();
}

}  // namespace wm
