// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/input_method_mus.h"

#include <utility>

#include "components/mus/public/cpp/window.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/platform_window/mojo/ime_type_converters.h"
#include "ui/platform_window/mojo/text_input_state.mojom.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// InputMethodMUS, public:

InputMethodMUS::InputMethodMUS(ui::internal::InputMethodDelegate* delegate,
                               mus::Window* window)
    : window_(window) {
  SetDelegate(delegate);
}

InputMethodMUS::~InputMethodMUS() {}

////////////////////////////////////////////////////////////////////////////////
// InputMethodMUS, ui::InputMethod implementation:

void InputMethodMUS::OnFocus() {
  InputMethodBase::OnFocus();
  UpdateTextInputType();
}

void InputMethodMUS::OnBlur() {
  InputMethodBase::OnBlur();
  UpdateTextInputType();
}

bool InputMethodMUS::OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                              NativeEventResult* result) {
  return false;
}

void InputMethodMUS::DispatchKeyEvent(ui::KeyEvent* event) {
  DCHECK(event->type() == ui::ET_KEY_PRESSED ||
         event->type() == ui::ET_KEY_RELEASED);

  // If no text input client, do nothing.
  if (!GetTextInputClient()) {
    ignore_result(DispatchKeyEventPostIME(event));
    return;
  }

  // Here is where we change the differ from our base class's logic. Instead of
  // always dispatching a key down event, and then sending a synthesized
  // character event, we instead check to see if this is a character event and
  // send out the key if it is. (We fallback to normal dispatch if it isn't.)
  if (event->is_char()) {
    GetTextInputClient()->InsertChar(*event);
    event->StopPropagation();
    return;
  }

  ignore_result(DispatchKeyEventPostIME(event));
}

void InputMethodMUS::OnTextInputTypeChanged(const ui::TextInputClient* client) {
  if (IsTextInputClientFocused(client))
    UpdateTextInputType();
  InputMethodBase::OnTextInputTypeChanged(client);
}

void InputMethodMUS::OnCaretBoundsChanged(const ui::TextInputClient* client) {}

void InputMethodMUS::CancelComposition(const ui::TextInputClient* client) {}

void InputMethodMUS::OnInputLocaleChanged() {}

std::string InputMethodMUS::GetInputLocale() {
  return "";
}

bool InputMethodMUS::IsCandidatePopupOpen() const {
  return false;
}

void InputMethodMUS::OnDidChangeFocusedClient(
    ui::TextInputClient* focused_before,
    ui::TextInputClient* focused) {
  InputMethodBase::OnDidChangeFocusedClient(focused_before, focused);
  UpdateTextInputType();
}

void InputMethodMUS::UpdateTextInputType() {
  ui::TextInputType type = GetTextInputType();
  mojo::TextInputStatePtr state = mojo::TextInputState::New();
  state->type = mojo::ConvertTo<mojo::TextInputType>(type);
  if (type != ui::TEXT_INPUT_TYPE_NONE)
    window_->SetImeVisibility(true, std::move(state));
  else
    window_->SetTextInputState(std::move(state));
}

}  // namespace views
