// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/ime/input_method_bridge.h"

#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/events/event.h"
#include "ui/gfx/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

// InputMethodBridge::HostObserver class ---------------------------------------

// An observer class for observing the host input method. When the host input
// method is destroyed, it will null out the |host_| field on the
// InputMethodBridge object.
class InputMethodBridge::HostObserver : public ui::InputMethodObserver {
 public:
  explicit HostObserver(InputMethodBridge* bridge);
  virtual ~HostObserver();

  virtual void OnTextInputTypeChanged(
      const ui::TextInputClient* client) OVERRIDE {}
  virtual void OnFocus() OVERRIDE {}
  virtual void OnBlur() OVERRIDE {}
  virtual void OnCaretBoundsChanged(
      const ui::TextInputClient* client) OVERRIDE {}
  virtual void OnTextInputStateChanged(
      const ui::TextInputClient* client) OVERRIDE {}
  virtual void OnInputMethodDestroyed(
      const ui::InputMethod* input_method) OVERRIDE;
  virtual void OnShowImeIfNeeded() OVERRIDE {}

 private:
  InputMethodBridge* bridge_;

  DISALLOW_COPY_AND_ASSIGN(HostObserver);
};

InputMethodBridge::HostObserver::HostObserver(InputMethodBridge* bridge)
    : bridge_(bridge) {
  bridge_->host_->AddObserver(this);
}

InputMethodBridge::HostObserver::~HostObserver() {
  if (bridge_->host_)
    bridge_->host_->RemoveObserver(this);
}

void InputMethodBridge::HostObserver::OnInputMethodDestroyed(
    const ui::InputMethod* input_method) {
  DCHECK_EQ(bridge_->host_, input_method);
  bridge_->host_->RemoveObserver(this);
  bridge_->host_ = NULL;
}

// InputMethodBridge class -----------------------------------------------------

InputMethodBridge::InputMethodBridge(internal::InputMethodDelegate* delegate,
                                     ui::InputMethod* host,
                                     bool shared_input_method)
    : host_(host),
      shared_input_method_(shared_input_method) {
  DCHECK(host_);
  SetDelegate(delegate);

  host_observer_.reset(new HostObserver(this));
}

InputMethodBridge::~InputMethodBridge() {
  // By the time we get here it's very likely |widget_|'s NativeWidget has been
  // destroyed. This means any calls to |widget_| that go to the NativeWidget,
  // such as IsActive(), will crash. SetFocusedTextInputClient() may callback to
  // this and go into |widget_|. NULL out |widget_| so we don't attempt to use
  // it.
  DetachFromWidget();

  // Host input method might have been destroyed at this point.
  if (host_)
    host_->DetachTextInputClient(this);
}

void InputMethodBridge::OnFocus() {
  DCHECK(host_);

  // Direct the shared IME to send TextInputClient messages to |this| object.
  if (shared_input_method_ || !host_->GetTextInputClient())
    host_->SetFocusedTextInputClient(this);

  // TODO(yusukes): We don't need to call OnTextInputTypeChanged() once we move
  // text input type tracker code to ui::InputMethodBase.
  if (GetFocusedView()) {
    OnTextInputTypeChanged(GetFocusedView());
    OnCaretBoundsChanged(GetFocusedView());
  }
}

void InputMethodBridge::OnBlur() {
  DCHECK(host_);

  if (HasCompositionText()) {
    ConfirmCompositionText();
    host_->CancelComposition(this);
  }

  if (host_->GetTextInputClient() == this)
    host_->SetFocusedTextInputClient(NULL);
}

bool InputMethodBridge::OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                                 NativeEventResult* result) {
  DCHECK(host_);

  return host_->OnUntranslatedIMEMessage(event, result);
}

void InputMethodBridge::DispatchKeyEvent(const ui::KeyEvent& key) {
  DCHECK(key.type() == ui::ET_KEY_PRESSED || key.type() == ui::ET_KEY_RELEASED);

  // We can just dispatch the event here since the |key| is already processed by
  // the system-wide IME.
  DispatchKeyEventPostIME(key);
}

void InputMethodBridge::OnTextInputTypeChanged(View* view) {
  DCHECK(host_);

  if (IsViewFocused(view))
    host_->OnTextInputTypeChanged(this);
  InputMethodBase::OnTextInputTypeChanged(view);
}

void InputMethodBridge::OnCaretBoundsChanged(View* view) {
  DCHECK(host_);

  if (IsViewFocused(view) && !IsTextInputTypeNone())
    host_->OnCaretBoundsChanged(this);
}

void InputMethodBridge::CancelComposition(View* view) {
  DCHECK(host_);

  if (IsViewFocused(view))
    host_->CancelComposition(this);
}

void InputMethodBridge::OnInputLocaleChanged() {
  DCHECK(host_);

  host_->OnInputLocaleChanged();
}

std::string InputMethodBridge::GetInputLocale() {
  DCHECK(host_);

  return host_->GetInputLocale();
}

bool InputMethodBridge::IsActive() {
  DCHECK(host_);

  return host_->IsActive();
}

bool InputMethodBridge::IsCandidatePopupOpen() const {
  DCHECK(host_);

  return host_->IsCandidatePopupOpen();
}

void InputMethodBridge::ShowImeIfNeeded() {
  DCHECK(host_);
  host_->ShowImeIfNeeded();
}

// Overridden from TextInputClient. Forward an event from the system-wide IME
// to the text input |client|, which is e.g. views::Textfield.
void InputMethodBridge::SetCompositionText(
    const ui::CompositionText& composition) {
  TextInputClient* client = GetTextInputClient();
  if (client)
    client->SetCompositionText(composition);
}

void InputMethodBridge::ConfirmCompositionText() {
  TextInputClient* client = GetTextInputClient();
  if (client)
    client->ConfirmCompositionText();
}

void InputMethodBridge::ClearCompositionText() {
  TextInputClient* client = GetTextInputClient();
  if (client)
    client->ClearCompositionText();
}

void InputMethodBridge::InsertText(const base::string16& text) {
  TextInputClient* client = GetTextInputClient();
  if (client)
    client->InsertText(text);
}

void InputMethodBridge::InsertChar(base::char16 ch, int flags) {
  TextInputClient* client = GetTextInputClient();
  if (client)
    client->InsertChar(ch, flags);
}

gfx::NativeWindow InputMethodBridge::GetAttachedWindow() const {
  TextInputClient* client = GetTextInputClient();
  return client ?
      client->GetAttachedWindow() : static_cast<gfx::NativeWindow>(NULL);
}

ui::TextInputType InputMethodBridge::GetTextInputType() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetTextInputType() : ui::TEXT_INPUT_TYPE_NONE;
}

ui::TextInputMode InputMethodBridge::GetTextInputMode() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetTextInputMode() : ui::TEXT_INPUT_MODE_DEFAULT;
}

bool InputMethodBridge::CanComposeInline() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->CanComposeInline() : true;
}

gfx::Rect InputMethodBridge::GetCaretBounds() const {
  TextInputClient* client = GetTextInputClient();
  if (!client)
    return gfx::Rect();

  return client->GetCaretBounds();
}

bool InputMethodBridge::GetCompositionCharacterBounds(uint32 index,
                                                      gfx::Rect* rect) const {
  DCHECK(rect);
  TextInputClient* client = GetTextInputClient();
  if (!client)
    return false;

  return client->GetCompositionCharacterBounds(index, rect);
}

bool InputMethodBridge::HasCompositionText() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->HasCompositionText() : false;
}

bool InputMethodBridge::GetTextRange(gfx::Range* range) const {
  TextInputClient* client = GetTextInputClient();
  return client ?  client->GetTextRange(range) : false;
}

bool InputMethodBridge::GetCompositionTextRange(gfx::Range* range) const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetCompositionTextRange(range) : false;
}

bool InputMethodBridge::GetSelectionRange(gfx::Range* range) const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetSelectionRange(range) : false;
}

bool InputMethodBridge::SetSelectionRange(const gfx::Range& range) {
  TextInputClient* client = GetTextInputClient();
  return client ? client->SetSelectionRange(range) : false;
}

bool InputMethodBridge::DeleteRange(const gfx::Range& range) {
  TextInputClient* client = GetTextInputClient();
  return client ? client->DeleteRange(range) : false;
}

bool InputMethodBridge::GetTextFromRange(const gfx::Range& range,
                                         base::string16* text) const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetTextFromRange(range, text) : false;
}

void InputMethodBridge::OnInputMethodChanged() {
  TextInputClient* client = GetTextInputClient();
  if (client)
    client->OnInputMethodChanged();
}

bool InputMethodBridge::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  TextInputClient* client = GetTextInputClient();
  return client ?
      client->ChangeTextDirectionAndLayoutAlignment(direction) : false;
}

void InputMethodBridge::ExtendSelectionAndDelete(size_t before, size_t after) {
  TextInputClient* client = GetTextInputClient();
  if (client)
    client->ExtendSelectionAndDelete(before, after);
}

void InputMethodBridge::EnsureCaretInRect(const gfx::Rect& rect) {
  TextInputClient* client = GetTextInputClient();
  if (client)
    client->EnsureCaretInRect(rect);
}

void InputMethodBridge::OnCandidateWindowShown() {
}

void InputMethodBridge::OnCandidateWindowUpdated() {
}

void InputMethodBridge::OnCandidateWindowHidden() {
}

bool InputMethodBridge::IsEditingCommandEnabled(int command_id) {
  return false;
}

void InputMethodBridge::ExecuteEditingCommand(int command_id) {
}

// Overridden from FocusChangeListener.
void InputMethodBridge::OnWillChangeFocus(View* focused_before, View* focused) {
  if (HasCompositionText()) {
    ConfirmCompositionText();
    CancelComposition(focused_before);
  }
}

void InputMethodBridge::OnDidChangeFocus(View* focused_before, View* focused) {
  DCHECK_EQ(GetFocusedView(), focused);
  OnTextInputTypeChanged(focused);
  OnCaretBoundsChanged(focused);
}

ui::InputMethod* InputMethodBridge::GetHostInputMethod() const {
  return host_;
}


}  // namespace views
