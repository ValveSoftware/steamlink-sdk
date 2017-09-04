// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/input_method_mus.h"

#include <utility>

#include "services/ui/public/cpp/window.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/ime.mojom.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/platform_window/mojo/ime_type_converters.h"
#include "ui/platform_window/mojo/text_input_state.mojom.h"
#include "ui/views/mus/text_input_client_impl.h"

using ui::mojom::EventResult;

namespace views {

////////////////////////////////////////////////////////////////////////////////
// InputMethodMus, public:

InputMethodMus::InputMethodMus(ui::internal::InputMethodDelegate* delegate,
                               ui::Window* window)
    : window_(window) {
  SetDelegate(delegate);
}

InputMethodMus::~InputMethodMus() {}

void InputMethodMus::Init(service_manager::Connector* connector) {
  connector->ConnectToInterface(ui::mojom::kServiceName, &ime_server_);
}

void InputMethodMus::DispatchKeyEvent(
    ui::KeyEvent* event,
    std::unique_ptr<base::Callback<void(EventResult)>> ack_callback) {
  DCHECK(event->type() == ui::ET_KEY_PRESSED ||
         event->type() == ui::ET_KEY_RELEASED);

  // If no text input client, do nothing.
  if (!GetTextInputClient()) {
    ignore_result(DispatchKeyEventPostIME(event));
    if (ack_callback) {
      ack_callback->Run(event->handled() ? EventResult::HANDLED
                                         : EventResult::UNHANDLED);
    }
    return;
  }

  // IME driver will notify us whether it handled the event or not by calling
  // ProcessKeyEventCallback(), in which we will run the |ack_callback| to tell
  // the window server if client handled the event or not.
  input_method_->ProcessKeyEvent(
      ui::Event::Clone(*event),
      base::Bind(&InputMethodMus::ProcessKeyEventCallback,
                 base::Unretained(this), *event, Passed(&ack_callback)));
}

////////////////////////////////////////////////////////////////////////////////
// InputMethodMus, ui::InputMethod implementation:

void InputMethodMus::OnFocus() {
  InputMethodBase::OnFocus();
  UpdateTextInputType();
}

void InputMethodMus::OnBlur() {
  InputMethodBase::OnBlur();
  UpdateTextInputType();
}

bool InputMethodMus::OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                              NativeEventResult* result) {
  // This method is not called on non-Windows platforms. See the comments for
  // ui::InputMethod::OnUntranslatedIMEMessage().
  return false;
}

void InputMethodMus::DispatchKeyEvent(ui::KeyEvent* event) {
  DispatchKeyEvent(event, nullptr);
}

void InputMethodMus::OnTextInputTypeChanged(const ui::TextInputClient* client) {
  if (IsTextInputClientFocused(client))
    UpdateTextInputType();
  InputMethodBase::OnTextInputTypeChanged(client);

  if (input_method_) {
    input_method_->OnTextInputTypeChanged(
        static_cast<ui::mojom::TextInputType>(client->GetTextInputType()));
  }
}

void InputMethodMus::OnCaretBoundsChanged(const ui::TextInputClient* client) {
  if (input_method_)
    input_method_->OnCaretBoundsChanged(client->GetCaretBounds());
}

void InputMethodMus::CancelComposition(const ui::TextInputClient* client) {
  if (input_method_)
    input_method_->CancelComposition();
}

void InputMethodMus::OnInputLocaleChanged() {
  // TODO(moshayedi): crbug.com/637418. Not supported in ChromeOS. Investigate
  // whether we want to support this or not.
}

bool InputMethodMus::IsCandidatePopupOpen() const {
  // TODO(moshayedi): crbug.com/637416. Implement this properly when we have a
  // mean for displaying candidate list popup.
  return false;
}

void InputMethodMus::OnDidChangeFocusedClient(
    ui::TextInputClient* focused_before,
    ui::TextInputClient* focused) {
  InputMethodBase::OnDidChangeFocusedClient(focused_before, focused);
  UpdateTextInputType();

  text_input_client_ = base::MakeUnique<TextInputClientImpl>(focused);
  ime_server_->StartSession(text_input_client_->CreateInterfacePtrAndBind(),
                            GetProxy(&input_method_));
}

void InputMethodMus::UpdateTextInputType() {
  ui::TextInputType type = GetTextInputType();
  mojo::TextInputStatePtr state = mojo::TextInputState::New();
  state->type = mojo::ConvertTo<mojo::TextInputType>(type);
  if (window_) {
    if (type != ui::TEXT_INPUT_TYPE_NONE)
      window_->SetImeVisibility(true, std::move(state));
    else
      window_->SetTextInputState(std::move(state));
  }
}

void InputMethodMus::ProcessKeyEventCallback(
    const ui::KeyEvent& event,
    std::unique_ptr<base::Callback<void(EventResult)>> ack_callback,
    bool handled) {
  EventResult event_result;
  if (!handled) {
    // If not handled by IME, try dispatching the event to delegate to see if
    // any client-side post-ime processing needs to be done. This includes cases
    // like backspace, return key, etc.
    std::unique_ptr<ui::Event> event_clone = ui::Event::Clone(event);
    ignore_result(DispatchKeyEventPostIME(event_clone->AsKeyEvent()));
    event_result =
        event_clone->handled() ? EventResult::HANDLED : EventResult::UNHANDLED;
  } else {
    event_result = EventResult::HANDLED;
  }
  // |ack_callback| can be null if the standard form of DispatchKeyEvent() is
  // called instead of the version which provides a callback. In mus+ash we
  // use the version with callback, but some unittests use the standard form.
  if (ack_callback)
    ack_callback->Run(event_result);
}

}  // namespace views
