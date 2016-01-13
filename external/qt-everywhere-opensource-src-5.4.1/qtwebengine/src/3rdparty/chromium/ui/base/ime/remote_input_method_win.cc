// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/remote_input_method_win.h"

#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/metro.h"
#include "base/win/scoped_handle.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/remote_input_method_delegate_win.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/win/tsf_input_scope.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/rect.h"

namespace ui {
namespace {

const LANGID kFallbackLangID =
    MAKELANGID(LANG_NEUTRAL, SUBLANG_UI_CUSTOM_DEFAULT);

InputMethod* g_public_interface_ = NULL;
RemoteInputMethodPrivateWin* g_private_interface_ = NULL;

void RegisterInstance(InputMethod* public_interface,
                      RemoteInputMethodPrivateWin* private_interface) {
  CHECK(g_public_interface_ == NULL)
      << "Only one instance is supported at the same time";
  CHECK(g_private_interface_ == NULL)
      << "Only one instance is supported at the same time";
  g_public_interface_ = public_interface;
  g_private_interface_ = private_interface;
}

RemoteInputMethodPrivateWin* GetPrivate(InputMethod* public_interface) {
  if (g_public_interface_ != public_interface)
    return NULL;
  return g_private_interface_;
}

void UnregisterInstance(InputMethod* public_interface) {
  RemoteInputMethodPrivateWin* private_interface = GetPrivate(public_interface);
  if (g_public_interface_ == public_interface &&
      g_private_interface_ == private_interface) {
    g_public_interface_ = NULL;
    g_private_interface_ = NULL;
  }
}

std::string GetLocaleString(LCID Locale_id, LCTYPE locale_type) {
  wchar_t buffer[16] = {};

  //|chars_written| includes NUL terminator.
  const int chars_written =
      GetLocaleInfo(Locale_id, locale_type, buffer, arraysize(buffer));
  if (chars_written <= 1 || arraysize(buffer) < chars_written)
    return std::string();
  std::string result;
  base::WideToUTF8(buffer, chars_written - 1, &result);
  return result;
}

std::vector<int32> GetInputScopesAsInt(TextInputType text_input_type,
                                       TextInputMode text_input_mode) {
  std::vector<int32> result;
  // An empty vector represents |text_input_type| is TEXT_INPUT_TYPE_NONE.
  if (text_input_type == TEXT_INPUT_TYPE_NONE)
    return result;

  const std::vector<InputScope>& input_scopes =
      tsf_inputscope::GetInputScopes(text_input_type, text_input_mode);
  result.reserve(input_scopes.size());
  for (size_t i = 0; i < input_scopes.size(); ++i)
    result.push_back(static_cast<int32>(input_scopes[i]));
  return result;
}

std::vector<gfx::Rect> GetCompositionCharacterBounds(
    const TextInputClient* client) {
  if (!client)
    return std::vector<gfx::Rect>();

  std::vector<gfx::Rect> bounds;
  if (client->HasCompositionText()) {
    gfx::Range range;
    if (client->GetCompositionTextRange(&range)) {
      for (uint32 i = 0; i < range.length(); ++i) {
        gfx::Rect rect;
        if (!client->GetCompositionCharacterBounds(i, &rect))
          break;
        bounds.push_back(rect);
      }
    }
  }

  // Use the caret bounds as a fallback if no composition character bounds is
  // available. One typical use case is PPAPI Flash, which does not support
  // GetCompositionCharacterBounds at all. crbug.com/133472
  if (bounds.empty())
    bounds.push_back(client->GetCaretBounds());
  return bounds;
}

class RemoteInputMethodWin : public InputMethod,
                             public RemoteInputMethodPrivateWin {
 public:
  explicit RemoteInputMethodWin(internal::InputMethodDelegate* delegate)
      : delegate_(delegate),
        remote_delegate_(NULL),
        text_input_client_(NULL),
        is_candidate_popup_open_(false),
        is_ime_(false),
        langid_(kFallbackLangID) {
    RegisterInstance(this, this);
  }

  virtual ~RemoteInputMethodWin() {
    FOR_EACH_OBSERVER(InputMethodObserver,
                      observer_list_,
                      OnInputMethodDestroyed(this));
    UnregisterInstance(this);
  }

 private:
  // Overridden from InputMethod:
  virtual void SetDelegate(internal::InputMethodDelegate* delegate) OVERRIDE {
    delegate_ = delegate;
  }

  virtual void Init(bool focused) OVERRIDE {
  }

  virtual void OnFocus() OVERRIDE {
  }

  virtual void OnBlur() OVERRIDE {
  }

  virtual bool OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                        NativeEventResult* result) OVERRIDE {
    return false;
  }

  virtual void SetFocusedTextInputClient(TextInputClient* client) OVERRIDE {
    std::vector<int32> prev_input_scopes;
    std::swap(input_scopes_, prev_input_scopes);
    std::vector<gfx::Rect> prev_bounds;
    std::swap(composition_character_bounds_, prev_bounds);
    if (client) {
      input_scopes_ = GetInputScopesAsInt(client->GetTextInputType(),
                                          client->GetTextInputMode());
      composition_character_bounds_ = GetCompositionCharacterBounds(client);
    }

    const bool text_input_client_changed = text_input_client_ != client;
    text_input_client_ = client;
    if (text_input_client_changed) {
      FOR_EACH_OBSERVER(InputMethodObserver,
                        observer_list_,
                        OnTextInputStateChanged(client));
    }

    if (!remote_delegate_ || (prev_input_scopes == input_scopes_ &&
                              prev_bounds == composition_character_bounds_))
      return;
    remote_delegate_->OnTextInputClientUpdated(input_scopes_,
                                               composition_character_bounds_);
  }

  virtual void DetachTextInputClient(TextInputClient* client) OVERRIDE {
    if (text_input_client_ != client)
      return;
    SetFocusedTextInputClient(NULL);
  }

  virtual TextInputClient* GetTextInputClient() const OVERRIDE {
    return text_input_client_;
  }

  virtual bool DispatchKeyEvent(const ui::KeyEvent& event) OVERRIDE {
    if (event.HasNativeEvent()) {
      const base::NativeEvent& native_key_event = event.native_event();
      if (native_key_event.message != WM_CHAR)
        return false;
      if (!text_input_client_)
        return false;
      text_input_client_->InsertChar(
          static_cast<base::char16>(native_key_event.wParam),
          ui::GetModifiersFromKeyState());
      return true;
    }

    if (event.is_char()) {
      if (text_input_client_) {
        text_input_client_->InsertChar(event.key_code(),
                                       ui::GetModifiersFromKeyState());
      }
      return true;
    }
    if (!delegate_)
      return false;
    return delegate_->DispatchKeyEventPostIME(event);
  }

  virtual void OnTextInputTypeChanged(const TextInputClient* client) OVERRIDE {
    if (!text_input_client_ || text_input_client_ != client)
      return;
    std::vector<int32> prev_input_scopes;
    std::swap(input_scopes_, prev_input_scopes);
    input_scopes_ = GetInputScopesAsInt(client->GetTextInputType(),
                                        client->GetTextInputMode());
    if (input_scopes_ != prev_input_scopes && remote_delegate_) {
      remote_delegate_->OnTextInputClientUpdated(
          input_scopes_, composition_character_bounds_);
    }
  }

  virtual void OnCaretBoundsChanged(const TextInputClient* client) OVERRIDE {
    if (!text_input_client_ || text_input_client_ != client)
      return;
    std::vector<gfx::Rect> prev_rects;
    std::swap(composition_character_bounds_, prev_rects);
    composition_character_bounds_ = GetCompositionCharacterBounds(client);
    if (composition_character_bounds_ != prev_rects && remote_delegate_) {
      remote_delegate_->OnTextInputClientUpdated(
          input_scopes_, composition_character_bounds_);
    }
  }

  virtual void CancelComposition(const TextInputClient* client) OVERRIDE {
    if (CanSendRemoteNotification(client))
      remote_delegate_->CancelComposition();
  }

  virtual void OnInputLocaleChanged() OVERRIDE {
  }

  virtual std::string GetInputLocale() OVERRIDE {
    const LCID locale_id = MAKELCID(langid_, SORT_DEFAULT);
    std::string language =
        GetLocaleString(locale_id, LOCALE_SISO639LANGNAME);
    if (SUBLANGID(langid_) == SUBLANG_NEUTRAL || language.empty())
      return language;
    const std::string& region =
        GetLocaleString(locale_id, LOCALE_SISO3166CTRYNAME);
    if (region.empty())
      return language;
    return language.append(1, '-').append(region);
  }

  virtual bool IsActive() OVERRIDE {
    return true;  // always turned on
  }

  virtual TextInputType GetTextInputType() const OVERRIDE {
    return text_input_client_ ? text_input_client_->GetTextInputType()
                              : TEXT_INPUT_TYPE_NONE;
  }

  virtual TextInputMode GetTextInputMode() const OVERRIDE {
    return text_input_client_ ? text_input_client_->GetTextInputMode()
                              : TEXT_INPUT_MODE_DEFAULT;
  }

  virtual bool CanComposeInline() const OVERRIDE {
    return text_input_client_ ? text_input_client_->CanComposeInline() : true;
  }

  virtual bool IsCandidatePopupOpen() const OVERRIDE {
    return is_candidate_popup_open_;
  }

  virtual void ShowImeIfNeeded() OVERRIDE {
  }

  virtual void AddObserver(InputMethodObserver* observer) OVERRIDE {
    observer_list_.AddObserver(observer);
  }

  virtual void RemoveObserver(InputMethodObserver* observer) OVERRIDE {
    observer_list_.RemoveObserver(observer);
  }

  // Overridden from RemoteInputMethodPrivateWin:
  virtual void SetRemoteDelegate(
      internal::RemoteInputMethodDelegateWin* delegate) OVERRIDE{
    remote_delegate_ = delegate;

    // Sync initial state.
    if (remote_delegate_) {
      remote_delegate_->OnTextInputClientUpdated(
          input_scopes_, composition_character_bounds_);
    }
  }

  virtual void OnCandidatePopupChanged(bool visible) OVERRIDE {
    is_candidate_popup_open_ = visible;
    if (!text_input_client_)
      return;
    // TODO(kochi): Support 'update' case, in addition to show/hide.
    // http://crbug.com/238585
    if (visible)
      text_input_client_->OnCandidateWindowShown();
    else
      text_input_client_->OnCandidateWindowHidden();
  }

  virtual void OnInputSourceChanged(LANGID langid, bool /*is_ime*/) OVERRIDE {
    // Note: Currently |is_ime| is not utilized yet.
    const bool changed = (langid_ != langid);
    langid_ = langid;
    if (changed && GetTextInputClient())
      GetTextInputClient()->OnInputMethodChanged();
  }

  virtual void OnCompositionChanged(
      const CompositionText& composition_text) OVERRIDE {
    if (!text_input_client_)
      return;
    text_input_client_->SetCompositionText(composition_text);
  }

  virtual void OnTextCommitted(const base::string16& text) OVERRIDE {
    if (!text_input_client_)
      return;
    if (text_input_client_->GetTextInputType() == TEXT_INPUT_TYPE_NONE) {
      // According to the comment in text_input_client.h,
      // TextInputClient::InsertText should never be called when the
      // text input type is TEXT_INPUT_TYPE_NONE.
      for (size_t i = 0; i < text.size(); ++i)
        text_input_client_->InsertChar(text[i], 0);
      return;
    }
    text_input_client_->InsertText(text);
  }

  bool CanSendRemoteNotification(
      const TextInputClient* text_input_client) const {
    return text_input_client_ &&
           text_input_client_ == text_input_client &&
           remote_delegate_;
  }

  ObserverList<InputMethodObserver> observer_list_;

  internal::InputMethodDelegate* delegate_;
  internal::RemoteInputMethodDelegateWin* remote_delegate_;

  TextInputClient* text_input_client_;
  std::vector<int32> input_scopes_;
  std::vector<gfx::Rect> composition_character_bounds_;
  bool is_candidate_popup_open_;
  bool is_ime_;
  LANGID langid_;

  DISALLOW_COPY_AND_ASSIGN(RemoteInputMethodWin);
};

}  // namespace

bool IsRemoteInputMethodWinRequired(gfx::AcceleratedWidget widget) {
  DWORD process_id = 0;
  if (GetWindowThreadProcessId(widget, &process_id) == 0)
    return false;
  base::win::ScopedHandle process_handle(::OpenProcess(
      PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id));
  if (!process_handle.IsValid())
    return false;
  return base::win::IsProcessImmersive(process_handle.Get());
}

RemoteInputMethodPrivateWin::RemoteInputMethodPrivateWin() {}

scoped_ptr<InputMethod> CreateRemoteInputMethodWin(
    internal::InputMethodDelegate* delegate) {
  return scoped_ptr<InputMethod>(new RemoteInputMethodWin(delegate));
}

// static
RemoteInputMethodPrivateWin* RemoteInputMethodPrivateWin::Get(
    InputMethod* input_method) {
  return GetPrivate(input_method);
}

}  // namespace ui
