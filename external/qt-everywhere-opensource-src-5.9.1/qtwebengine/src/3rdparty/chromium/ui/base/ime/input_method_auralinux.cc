// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_auralinux.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/environment.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/ime_engine_handler_interface.h"
#include "ui/base/ime/linux/linux_input_method_context_factory.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"

namespace {

ui::IMEEngineHandlerInterface* GetEngine() {
  if (ui::IMEBridge::Get())
    return ui::IMEBridge::Get()->GetCurrentEngineHandler();
  return nullptr;
}

}  // namespace

namespace ui {

InputMethodAuraLinux::InputMethodAuraLinux(
    internal::InputMethodDelegate* delegate)
    : text_input_type_(TEXT_INPUT_TYPE_NONE),
      is_sync_mode_(false),
      composition_changed_(false),
      suppress_next_result_(false),
      weak_ptr_factory_(this) {
  SetDelegate(delegate);
  context_ =
      LinuxInputMethodContextFactory::instance()->CreateInputMethodContext(
          this, false);
  context_simple_ =
      LinuxInputMethodContextFactory::instance()->CreateInputMethodContext(
          this, true);
}

InputMethodAuraLinux::~InputMethodAuraLinux() {
}

LinuxInputMethodContext* InputMethodAuraLinux::GetContextForTesting(
    bool is_simple) {
  return is_simple ? context_simple_.get() : context_.get();
}

// Overriden from InputMethod.

bool InputMethodAuraLinux::OnUntranslatedIMEMessage(
    const base::NativeEvent& event,
    NativeEventResult* result) {
  return false;
}

void InputMethodAuraLinux::DispatchKeyEvent(ui::KeyEvent* event) {
  DCHECK(event->type() == ET_KEY_PRESSED || event->type() == ET_KEY_RELEASED);

  // If no text input client, do nothing.
  if (!GetTextInputClient()) {
    ignore_result(DispatchKeyEventPostIME(event));
    return;
  }

  if (!event->HasNativeEvent() && sending_key_event_) {
    // Faked key events that are sent from input.ime.sendKeyEvents.
    ui::EventDispatchDetails details = DispatchKeyEventPostIME(event);
    if (details.dispatcher_destroyed || details.target_destroyed ||
        event->stopped_propagation()) {
      return;
    }
    if ((event->is_char() || event->GetDomKey().IsCharacter()) &&
        event->type() == ui::ET_KEY_PRESSED) {
      GetTextInputClient()->InsertChar(*event);
    }
    return;
  }

  suppress_next_result_ = false;
  composition_changed_ = false;
  result_text_.clear();

  bool filtered = false;
  {
    base::AutoReset<bool> flipper(&is_sync_mode_, true);
    if (text_input_type_ != TEXT_INPUT_TYPE_NONE &&
        text_input_type_ != TEXT_INPUT_TYPE_PASSWORD) {
      filtered = context_->DispatchKeyEvent(*event);
    } else {
      filtered = context_simple_->DispatchKeyEvent(*event);
    }
  }

  // If there's an active IME extension is listening to the key event, and the
  // current text input client is not password input client, the key event
  // should be dispatched to the extension engine in the two conditions:
  // 1) |filtered| == false: the ET_KEY_PRESSED event of non-character key,
  // or the ET_KEY_RELEASED event of all key.
  // 2) |filtered| == true && NeedInsertChar(): the ET_KEY_PRESSED event of
  // character key.
  if (text_input_type_ != TEXT_INPUT_TYPE_PASSWORD &&
      GetEngine() && GetEngine()->IsInterestedInKeyEvent() &&
      (!filtered || NeedInsertChar())) {
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback = base::Bind(
        &InputMethodAuraLinux::ProcessKeyEventByEngineDone,
        weak_ptr_factory_.GetWeakPtr(), base::Owned(new ui::KeyEvent(*event)),
        filtered, composition_changed_,
        base::Owned(new ui::CompositionText(composition_)),
        base::Owned(new base::string16(result_text_)));
    GetEngine()->ProcessKeyEvent(*event, callback);
  } else {
    ProcessKeyEventDone(event, filtered, false);
  }
}

void InputMethodAuraLinux::ProcessKeyEventByEngineDone(
    ui::KeyEvent* event,
    bool filtered,
    bool composition_changed,
    ui::CompositionText* composition,
    base::string16* result_text,
    bool is_handled) {
  composition_changed_ = composition_changed;
  composition_.CopyFrom(*composition);
  result_text_ = *result_text;
  ProcessKeyEventDone(event, filtered, is_handled);
}

void InputMethodAuraLinux::ProcessKeyEventDone(ui::KeyEvent* event,
                                               bool filtered,
                                               bool is_handled) {
  DCHECK(event);
  if (is_handled)
    return;

  // If the IME extension has not handled the key event, passes the keyevent
  // back to the previous processing flow. Preconditions for this situation:
  // 1) |filtered| == false
  // 2) |filtered| == true && NeedInsertChar()
  ui::EventDispatchDetails details;
  if (event->type() == ui::ET_KEY_PRESSED && filtered) {
    if (NeedInsertChar())
      details = DispatchKeyEventPostIME(event);
    else if (HasInputMethodResult())
      details = SendFakeProcessKeyEvent(event);
    if (details.dispatcher_destroyed)
      return;
    // If the KEYDOWN is stopped propagation (e.g. triggered an accelerator),
    // don't InsertChar/InsertText to the input field.
    if (event->stopped_propagation() || details.target_destroyed) {
      ResetContext();
      return;
    }

    // Don't send VKEY_PROCESSKEY event if there is no result text or
    // composition. This is to workaround the weird behavior of IBus with US
    // keyboard, which mutes the keydown and later fake a new keydown with IME
    // result in sync mode. In that case, user would expect only
    // keydown/keypress/keyup event without an initial 229 keydown event.
  }

  bool should_stop_propagation = false;
  // Note: |client| could be NULL because DispatchKeyEventPostIME could have
  // changed the text input client.
  TextInputClient* client = GetTextInputClient();
  // Processes the result text before composition for sync mode.
  if (client && !result_text_.empty()) {
    if (filtered && NeedInsertChar()) {
      for (const auto ch : result_text_) {
        ui::KeyEvent ch_event(*event);
        ch_event.set_character(ch);
        client->InsertChar(ch_event);
      }
    } else {
      // If |filtered| is false, that means the IME wants to commit some text
      // but still release the key to the application. For example, Korean IME
      // handles ENTER key to confirm its composition but still release it for
      // the default behavior (e.g. trigger search, etc.)
      // In such case, don't do InsertChar because a key should only trigger the
      // keydown event once.
      client->InsertText(result_text_);
    }
    should_stop_propagation = true;
  }

  if (client && composition_changed_ && !IsTextInputTypeNone()) {
    // If composition changed, does SetComposition if composition is not empty.
    // And ClearComposition if composition is empty.
    if (!composition_.text.empty())
      client->SetCompositionText(composition_);
    else if (result_text_.empty())
      client->ClearCompositionText();
    should_stop_propagation = true;
  }

  // Makes sure the cached composition is cleared after committing any text or
  // cleared composition.
  if (client && !client->HasCompositionText())
    composition_.Clear();

  if (!filtered) {
    details = DispatchKeyEventPostIME(event);
    if (details.dispatcher_destroyed) {
      if (should_stop_propagation)
        event->StopPropagation();
      return;
    }
    if (event->stopped_propagation() || details.target_destroyed) {
      ResetContext();
    } else if (event->type() == ui::ET_KEY_PRESSED) {
      // If a key event was not filtered by |context_| or |context_simple_|,
      // then it means the key event didn't generate any result text. For some
      // cases, the key event may still generate a valid character, eg. a
      // control-key event (ctrl-a, return, tab, etc.). We need to send the
      // character to the focused text input client by calling
      // TextInputClient::InsertChar().
      // Note: don't use |client| and use GetTextInputClient() here because
      // DispatchKeyEventPostIME may cause the current text input client change.
      base::char16 ch = event->GetCharacter();
      if (ch && GetTextInputClient())
        GetTextInputClient()->InsertChar(*event);
      should_stop_propagation = true;
    }
  }

  if (should_stop_propagation)
    event->StopPropagation();
}

void InputMethodAuraLinux::UpdateContextFocusState() {
  bool old_text_input_type = text_input_type_;
  text_input_type_ = GetTextInputType();

  // We only focus in |context_| when the focus is in a textfield.
  if (old_text_input_type != TEXT_INPUT_TYPE_NONE &&
      text_input_type_ == TEXT_INPUT_TYPE_NONE) {
    context_->Blur();
  } else if (old_text_input_type == TEXT_INPUT_TYPE_NONE &&
             text_input_type_ != TEXT_INPUT_TYPE_NONE) {
    context_->Focus();
  }

  // |context_simple_| can be used in any textfield, including password box, and
  // even if the focused text input client's text input type is
  // ui::TEXT_INPUT_TYPE_NONE.
  if (GetTextInputClient())
    context_simple_->Focus();
  else
    context_simple_->Blur();

  if (!ui::IMEBridge::Get())  // IMEBridge could be null for tests.
    return;

  ui::IMEEngineHandlerInterface::InputContext context(
      GetTextInputType(), GetTextInputMode(), GetTextInputFlags());
  ui::IMEBridge::Get()->SetCurrentInputContext(context);

  ui::IMEEngineHandlerInterface* engine = GetEngine();
  if (engine) {
    if (old_text_input_type != TEXT_INPUT_TYPE_NONE)
      engine->FocusOut();
    if (text_input_type_ != TEXT_INPUT_TYPE_NONE)
      engine->FocusIn(context);
  }
}

void InputMethodAuraLinux::OnTextInputTypeChanged(
    const TextInputClient* client) {
  UpdateContextFocusState();
  InputMethodBase::OnTextInputTypeChanged(client);
  // TODO(yoichio): Support inputmode HTML attribute.
}

void InputMethodAuraLinux::OnCaretBoundsChanged(const TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;
  NotifyTextInputCaretBoundsChanged(client);
  context_->SetCursorLocation(GetTextInputClient()->GetCaretBounds());

  if (!IsTextInputTypeNone() && text_input_type_ != TEXT_INPUT_TYPE_PASSWORD &&
      GetEngine())
    GetEngine()->SetCompositionBounds(GetCompositionBounds(client));
}

void InputMethodAuraLinux::CancelComposition(const TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;

  if (GetEngine())
    GetEngine()->Reset();

  ResetContext();
}

void InputMethodAuraLinux::ResetContext() {
  if (!GetTextInputClient())
    return;

  // To prevent any text from being committed when resetting the |context_|;
  is_sync_mode_ = true;
  suppress_next_result_ = true;

  context_->Reset();
  context_simple_->Reset();

  // Some input methods may not honour the reset call. Focusing out/in the
  // |context_| to make sure it gets reset correctly.
  if (text_input_type_ != TEXT_INPUT_TYPE_NONE) {
    context_->Blur();
    context_->Focus();
  }

  composition_.Clear();
  result_text_.clear();
  is_sync_mode_ = false;
  composition_changed_ = false;
}

bool InputMethodAuraLinux::IsCandidatePopupOpen() const {
  // There seems no way to detect candidate windows or any popups.
  return false;
}

// Overriden from ui::LinuxInputMethodContextDelegate

void InputMethodAuraLinux::OnCommit(const base::string16& text) {
  if (suppress_next_result_ || !GetTextInputClient()) {
    suppress_next_result_ = false;
    return;
  }

  if (is_sync_mode_) {
    // Append the text to the buffer, because commit signal might be fired
    // multiple times when processing a key event.
    result_text_.append(text);
  } else if (!IsTextInputTypeNone()) {
    // If we are not handling key event, do not bother sending text result if
    // the focused text input client does not support text input.
    ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_PROCESSKEY, 0);
    ui::EventDispatchDetails details = SendFakeProcessKeyEvent(&event);
    if (details.dispatcher_destroyed)
      return;
    if (!event.stopped_propagation() && !details.target_destroyed)
      GetTextInputClient()->InsertText(text);
    composition_.Clear();
  }
}

void InputMethodAuraLinux::OnPreeditChanged(
    const CompositionText& composition_text) {
  if (suppress_next_result_ || IsTextInputTypeNone())
    return;

  if (is_sync_mode_) {
    if (!composition_.text.empty() || !composition_text.text.empty())
      composition_changed_ = true;
  } else {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_PROCESSKEY, 0);
    ui::EventDispatchDetails details = SendFakeProcessKeyEvent(&event);
    if (details.dispatcher_destroyed)
      return;
    if (!event.stopped_propagation() && !details.target_destroyed)
      GetTextInputClient()->SetCompositionText(composition_text);
  }

  composition_ = composition_text;
}

void InputMethodAuraLinux::OnPreeditEnd() {
  if (suppress_next_result_ || IsTextInputTypeNone())
    return;

  if (is_sync_mode_) {
    if (!composition_.text.empty()) {
      composition_.Clear();
      composition_changed_ = true;
    }
  } else {
    TextInputClient* client = GetTextInputClient();
    if (client && client->HasCompositionText()) {
      ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_PROCESSKEY, 0);
      ui::EventDispatchDetails details = SendFakeProcessKeyEvent(&event);
      if (details.dispatcher_destroyed)
        return;
      if (!event.stopped_propagation() && !details.target_destroyed)
        client->ClearCompositionText();
    }
    composition_.Clear();
  }
}

// Overridden from InputMethodBase.

void InputMethodAuraLinux::OnWillChangeFocusedClient(
    TextInputClient* focused_before,
    TextInputClient* focused) {
  ConfirmCompositionText();
}

void InputMethodAuraLinux::OnDidChangeFocusedClient(
    TextInputClient* focused_before,
    TextInputClient* focused) {
  UpdateContextFocusState();

  // Force to update caret bounds, in case the View thinks that the caret
  // bounds has not changed.
  if (text_input_type_ != TEXT_INPUT_TYPE_NONE)
    OnCaretBoundsChanged(GetTextInputClient());

  InputMethodBase::OnDidChangeFocusedClient(focused_before, focused);
}

// private

bool InputMethodAuraLinux::HasInputMethodResult() {
  return !result_text_.empty() || composition_changed_;
}

bool InputMethodAuraLinux::NeedInsertChar() const {
  return IsTextInputTypeNone() ||
         (!composition_changed_ && composition_.text.empty() &&
          result_text_.length() == 1);
}

ui::EventDispatchDetails InputMethodAuraLinux::SendFakeProcessKeyEvent(
    ui::KeyEvent* event) const {
  KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_PROCESSKEY, event->flags());
  ui::EventDispatchDetails details = DispatchKeyEventPostIME(&key_event);
  if (key_event.stopped_propagation())
    event->StopPropagation();
  return details;
}

void InputMethodAuraLinux::ConfirmCompositionText() {
  TextInputClient* client = GetTextInputClient();
  if (client && client->HasCompositionText()) {
    client->ConfirmCompositionText();

    if (GetEngine())
      GetEngine()->Reset();
  }

  ResetContext();
}

}  // namespace ui
