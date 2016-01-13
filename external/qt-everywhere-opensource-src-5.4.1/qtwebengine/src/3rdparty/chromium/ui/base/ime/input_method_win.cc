// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_win.h"

#include "base/basictypes.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/win/tsf_input_scope.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/win/dpi.h"
#include "ui/gfx/win/hwnd_util.h"

namespace ui {
namespace {

// Extra number of chars before and after selection (or composition) range which
// is returned to IME for improving conversion accuracy.
static const size_t kExtraNumberOfChars = 20;

}  // namespace

InputMethodWin::InputMethodWin(internal::InputMethodDelegate* delegate,
                               HWND toplevel_window_handle)
    : toplevel_window_handle_(toplevel_window_handle),
      pending_requested_direction_(base::i18n::UNKNOWN_DIRECTION),
      accept_carriage_return_(false),
      active_(false),
      enabled_(false),
      is_candidate_popup_open_(false),
      composing_window_handle_(NULL) {
  SetDelegate(delegate);
  // In non-Aura environment, appropriate callbacks to OnFocus() and OnBlur()
  // are not implemented yet. To work around this limitation, here we use
  // "always focused" model.
  // TODO(ime): Fix the caller of OnFocus() and OnBlur() so that appropriate
  // focus event will be passed.
  InputMethodBase::OnFocus();
}

void InputMethodWin::Init(bool focused) {
  // Gets the initial input locale.
  OnInputLocaleChanged();

  InputMethodBase::Init(focused);
}

void InputMethodWin::OnFocus() {
  // Ignore OnFocus event for "always focused" model. See the comment in the
  // constructor.
  // TODO(ime): Implement OnFocus once the callers are fixed.
}

void InputMethodWin::OnBlur() {
  // Ignore OnBlur event for "always focused" model. See the comment in the
  // constructor.
  // TODO(ime): Implement OnFocus once the callers are fixed.
}

bool InputMethodWin::OnUntranslatedIMEMessage(
    const base::NativeEvent& event,
    InputMethod::NativeEventResult* result) {
  LRESULT original_result = 0;
  BOOL handled = FALSE;
  switch (event.message) {
    case WM_IME_SETCONTEXT:
      original_result = OnImeSetContext(
          event.hwnd, event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_IME_STARTCOMPOSITION:
      original_result = OnImeStartComposition(
          event.hwnd, event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_IME_COMPOSITION:
      original_result = OnImeComposition(
          event.hwnd, event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_IME_ENDCOMPOSITION:
      original_result = OnImeEndComposition(
          event.hwnd, event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_IME_REQUEST:
      original_result = OnImeRequest(
          event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_CHAR:
    case WM_SYSCHAR:
      original_result = OnChar(
          event.hwnd, event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_IME_NOTIFY:
      original_result = OnImeNotify(
          event.message, event.wParam, event.lParam, &handled);
      break;
    default:
      NOTREACHED() << "Unknown IME message:" << event.message;
      break;
  }
  if (result)
    *result = original_result;
  return !!handled;
}

bool InputMethodWin::DispatchKeyEvent(const ui::KeyEvent& event) {
  if (!event.HasNativeEvent())
    return DispatchFabricatedKeyEvent(event);

  const base::NativeEvent& native_key_event = event.native_event();
  if (native_key_event.message == WM_CHAR) {
    BOOL handled;
    OnChar(native_key_event.hwnd, native_key_event.message,
           native_key_event.wParam, native_key_event.lParam, &handled);
    return !!handled;  // Don't send WM_CHAR for post event processing.
  }
  // Handles ctrl-shift key to change text direction and layout alignment.
  if (ui::IMM32Manager::IsRTLKeyboardLayoutInstalled() &&
      !IsTextInputTypeNone()) {
    // TODO: shouldn't need to generate a KeyEvent here.
    const ui::KeyEvent key(native_key_event,
                           native_key_event.message == WM_CHAR);
    ui::KeyboardCode code = key.key_code();
    if (key.type() == ui::ET_KEY_PRESSED) {
      if (code == ui::VKEY_SHIFT) {
        base::i18n::TextDirection dir;
        if (ui::IMM32Manager::IsCtrlShiftPressed(&dir))
          pending_requested_direction_ = dir;
      } else if (code != ui::VKEY_CONTROL) {
        pending_requested_direction_ = base::i18n::UNKNOWN_DIRECTION;
      }
    } else if (key.type() == ui::ET_KEY_RELEASED &&
               (code == ui::VKEY_SHIFT || code == ui::VKEY_CONTROL) &&
               pending_requested_direction_ != base::i18n::UNKNOWN_DIRECTION) {
      GetTextInputClient()->ChangeTextDirectionAndLayoutAlignment(
          pending_requested_direction_);
      pending_requested_direction_ = base::i18n::UNKNOWN_DIRECTION;
    }
  }

  return DispatchKeyEventPostIME(event);
}

void InputMethodWin::OnTextInputTypeChanged(const TextInputClient* client) {
  if (!IsTextInputClientFocused(client) || !IsWindowFocused(client))
    return;
  imm32_manager_.CancelIME(GetAttachedWindowHandle(client));
  UpdateIMEState();
}

void InputMethodWin::OnCaretBoundsChanged(const TextInputClient* client) {
  if (!enabled_ || !IsTextInputClientFocused(client) ||
      !IsWindowFocused(client)) {
    return;
  }
  // The current text input type should not be NONE if |client| is focused.
  DCHECK(!IsTextInputTypeNone());
  // Tentatively assume that the returned value is DIP (Density Independent
  // Pixel). See the comment in text_input_client.h and http://crbug.com/360334.
  const gfx::Rect dip_screen_bounds(GetTextInputClient()->GetCaretBounds());
  const gfx::Rect screen_bounds = gfx::win::DIPToScreenRect(dip_screen_bounds);

  HWND attached_window = GetAttachedWindowHandle(client);
  // TODO(ime): see comment in TextInputClient::GetCaretBounds(), this
  // conversion shouldn't be necessary.
  RECT r = {};
  GetClientRect(attached_window, &r);
  POINT window_point = { screen_bounds.x(), screen_bounds.y() };
  ScreenToClient(attached_window, &window_point);
  gfx::Rect caret_rect(gfx::Point(window_point.x, window_point.y),
                       screen_bounds.size());
  imm32_manager_.UpdateCaretRect(attached_window, caret_rect);
}

void InputMethodWin::CancelComposition(const TextInputClient* client) {
  if (enabled_ && IsTextInputClientFocused(client))
    imm32_manager_.CancelIME(GetAttachedWindowHandle(client));
}

void InputMethodWin::OnInputLocaleChanged() {
  active_ = imm32_manager_.SetInputLanguage();
  locale_ = imm32_manager_.GetInputLanguageName();
  OnInputMethodChanged();
}

std::string InputMethodWin::GetInputLocale() {
  return locale_;
}

bool InputMethodWin::IsActive() {
  return active_;
}

bool InputMethodWin::IsCandidatePopupOpen() const {
  return is_candidate_popup_open_;
}

void InputMethodWin::OnWillChangeFocusedClient(TextInputClient* focused_before,
                                               TextInputClient* focused) {
  if (IsWindowFocused(focused_before))
    ConfirmCompositionText();
}

void InputMethodWin::OnDidChangeFocusedClient(
    TextInputClient* focused_before,
    TextInputClient* focused) {
  if (IsWindowFocused(focused)) {
    // Force to update the input type since client's TextInputStateChanged()
    // function might not be called if text input types before the client loses
    // focus and after it acquires focus again are the same.
    OnTextInputTypeChanged(focused);

    UpdateIMEState();

    // Force to update caret bounds, in case the client thinks that the caret
    // bounds has not changed.
    OnCaretBoundsChanged(focused);
  }
  if (focused_before != focused)
    accept_carriage_return_ = false;
}

LRESULT InputMethodWin::OnChar(HWND window_handle,
                               UINT message,
                               WPARAM wparam,
                               LPARAM lparam,
                               BOOL* handled) {
  *handled = TRUE;

  // We need to send character events to the focused text input client event if
  // its text input type is ui::TEXT_INPUT_TYPE_NONE.
  if (GetTextInputClient()) {
    const base::char16 kCarriageReturn = L'\r';
    const base::char16 ch = static_cast<base::char16>(wparam);
    // A mask to determine the previous key state from |lparam|. The value is 1
    // if the key is down before the message is sent, or it is 0 if the key is
    // up.
    const uint32 kPrevKeyDownBit = 0x40000000;
    if (ch == kCarriageReturn && !(lparam & kPrevKeyDownBit))
      accept_carriage_return_ = true;
    // Conditionally ignore '\r' events to work around crbug.com/319100.
    // TODO(yukawa, IME): Figure out long-term solution.
    if (ch != kCarriageReturn || accept_carriage_return_)
      GetTextInputClient()->InsertChar(ch, ui::GetModifiersFromKeyState());
  }

  // Explicitly show the system menu at a good location on [Alt]+[Space].
  // Note: Setting |handled| to FALSE for DefWindowProc triggering of the system
  //       menu causes undesirable titlebar artifacts in the classic theme.
  if (message == WM_SYSCHAR && wparam == VK_SPACE)
    gfx::ShowSystemMenu(window_handle);

  return 0;
}

LRESULT InputMethodWin::OnImeSetContext(HWND window_handle,
                                        UINT message,
                                        WPARAM wparam,
                                        LPARAM lparam,
                                        BOOL* handled) {
  if (!!wparam)
    imm32_manager_.CreateImeWindow(window_handle);

  OnInputMethodChanged();
  return imm32_manager_.SetImeWindowStyle(
      window_handle, message, wparam, lparam, handled);
}

LRESULT InputMethodWin::OnImeStartComposition(HWND window_handle,
                                              UINT message,
                                              WPARAM wparam,
                                              LPARAM lparam,
                                              BOOL* handled) {
  // We have to prevent WTL from calling ::DefWindowProc() because the function
  // calls ::ImmSetCompositionWindow() and ::ImmSetCandidateWindow() to
  // over-write the position of IME windows.
  *handled = TRUE;

  // Reset the composition status and create IME windows.
  composing_window_handle_ = window_handle;
  imm32_manager_.CreateImeWindow(window_handle);
  imm32_manager_.ResetComposition(window_handle);
  return 0;
}

LRESULT InputMethodWin::OnImeComposition(HWND window_handle,
                                         UINT message,
                                         WPARAM wparam,
                                         LPARAM lparam,
                                         BOOL* handled) {
  // We have to prevent WTL from calling ::DefWindowProc() because we do not
  // want for the IMM (Input Method Manager) to send WM_IME_CHAR messages.
  *handled = TRUE;

  // At first, update the position of the IME window.
  imm32_manager_.UpdateImeWindow(window_handle);

  // Retrieve the result string and its attributes of the ongoing composition
  // and send it to a renderer process.
  ui::CompositionText composition;
  if (imm32_manager_.GetResult(window_handle, lparam, &composition.text)) {
    if (!IsTextInputTypeNone())
      GetTextInputClient()->InsertText(composition.text);
    imm32_manager_.ResetComposition(window_handle);
    // Fall though and try reading the composition string.
    // Japanese IMEs send a message containing both GCS_RESULTSTR and
    // GCS_COMPSTR, which means an ongoing composition has been finished
    // by the start of another composition.
  }
  // Retrieve the composition string and its attributes of the ongoing
  // composition and send it to a renderer process.
  if (imm32_manager_.GetComposition(window_handle, lparam, &composition) &&
      !IsTextInputTypeNone())
    GetTextInputClient()->SetCompositionText(composition);

  return 0;
}

LRESULT InputMethodWin::OnImeEndComposition(HWND window_handle,
                                            UINT message,
                                            WPARAM wparam,
                                            LPARAM lparam,
                                            BOOL* handled) {
  // Let WTL call ::DefWindowProc() and release its resources.
  *handled = FALSE;

  composing_window_handle_ = NULL;

  if (!IsTextInputTypeNone() && GetTextInputClient()->HasCompositionText())
    GetTextInputClient()->ClearCompositionText();

  imm32_manager_.ResetComposition(window_handle);
  imm32_manager_.DestroyImeWindow(window_handle);
  return 0;
}

LRESULT InputMethodWin::OnImeNotify(UINT message,
                                    WPARAM wparam,
                                    LPARAM lparam,
                                    BOOL* handled) {
  *handled = FALSE;

  bool previous_state = is_candidate_popup_open_;

  // Update |is_candidate_popup_open_|, whether a candidate window is open.
  switch (wparam) {
  case IMN_OPENCANDIDATE:
    is_candidate_popup_open_ = true;
    if (!previous_state)
      OnCandidateWindowShown();
    break;
  case IMN_CLOSECANDIDATE:
    is_candidate_popup_open_ = false;
    if (previous_state)
      OnCandidateWindowHidden();
    break;
  case IMN_CHANGECANDIDATE:
    // TODO(kochi): The IME API expects this event to notify window size change,
    // while this may fire more often without window resize. There is no generic
    // way to get bounds of candidate window.
    OnCandidateWindowUpdated();
    break;
  }

  return 0;
}

LRESULT InputMethodWin::OnImeRequest(UINT message,
                                     WPARAM wparam,
                                     LPARAM lparam,
                                     BOOL* handled) {
  *handled = FALSE;

  // Should not receive WM_IME_REQUEST message, if IME is disabled.
  const ui::TextInputType type = GetTextInputType();
  if (type == ui::TEXT_INPUT_TYPE_NONE ||
      type == ui::TEXT_INPUT_TYPE_PASSWORD) {
    return 0;
  }

  switch (wparam) {
    case IMR_RECONVERTSTRING:
      *handled = TRUE;
      return OnReconvertString(reinterpret_cast<RECONVERTSTRING*>(lparam));
    case IMR_DOCUMENTFEED:
      *handled = TRUE;
      return OnDocumentFeed(reinterpret_cast<RECONVERTSTRING*>(lparam));
    case IMR_QUERYCHARPOSITION:
      *handled = TRUE;
      return OnQueryCharPosition(reinterpret_cast<IMECHARPOSITION*>(lparam));
    default:
      return 0;
  }
}

LRESULT InputMethodWin::OnDocumentFeed(RECONVERTSTRING* reconv) {
  ui::TextInputClient* client = GetTextInputClient();
  if (!client)
    return 0;

  gfx::Range text_range;
  if (!client->GetTextRange(&text_range) || text_range.is_empty())
    return 0;

  bool result = false;
  gfx::Range target_range;
  if (client->HasCompositionText())
    result = client->GetCompositionTextRange(&target_range);

  if (!result || target_range.is_empty()) {
    if (!client->GetSelectionRange(&target_range) ||
        !target_range.IsValid()) {
      return 0;
    }
  }

  if (!text_range.Contains(target_range))
    return 0;

  if (target_range.GetMin() - text_range.start() > kExtraNumberOfChars)
    text_range.set_start(target_range.GetMin() - kExtraNumberOfChars);

  if (text_range.end() - target_range.GetMax() > kExtraNumberOfChars)
    text_range.set_end(target_range.GetMax() + kExtraNumberOfChars);

  size_t len = text_range.length();
  size_t need_size = sizeof(RECONVERTSTRING) + len * sizeof(WCHAR);

  if (!reconv)
    return need_size;

  if (reconv->dwSize < need_size)
    return 0;

  base::string16 text;
  if (!GetTextInputClient()->GetTextFromRange(text_range, &text))
    return 0;
  DCHECK_EQ(text_range.length(), text.length());

  reconv->dwVersion = 0;
  reconv->dwStrLen = len;
  reconv->dwStrOffset = sizeof(RECONVERTSTRING);
  reconv->dwCompStrLen =
      client->HasCompositionText() ? target_range.length() : 0;
  reconv->dwCompStrOffset =
      (target_range.GetMin() - text_range.start()) * sizeof(WCHAR);
  reconv->dwTargetStrLen = target_range.length();
  reconv->dwTargetStrOffset = reconv->dwCompStrOffset;

  memcpy((char*)reconv + sizeof(RECONVERTSTRING),
         text.c_str(), len * sizeof(WCHAR));

  // According to Microsoft API document, IMR_RECONVERTSTRING and
  // IMR_DOCUMENTFEED should return reconv, but some applications return
  // need_size.
  return reinterpret_cast<LRESULT>(reconv);
}

LRESULT InputMethodWin::OnReconvertString(RECONVERTSTRING* reconv) {
  ui::TextInputClient* client = GetTextInputClient();
  if (!client)
    return 0;

  // If there is a composition string already, we don't allow reconversion.
  if (client->HasCompositionText())
    return 0;

  gfx::Range text_range;
  if (!client->GetTextRange(&text_range) || text_range.is_empty())
    return 0;

  gfx::Range selection_range;
  if (!client->GetSelectionRange(&selection_range) ||
      selection_range.is_empty()) {
    return 0;
  }

  DCHECK(text_range.Contains(selection_range));

  size_t len = selection_range.length();
  size_t need_size = sizeof(RECONVERTSTRING) + len * sizeof(WCHAR);

  if (!reconv)
    return need_size;

  if (reconv->dwSize < need_size)
    return 0;

  // TODO(penghuang): Return some extra context to help improve IME's
  // reconversion accuracy.
  base::string16 text;
  if (!GetTextInputClient()->GetTextFromRange(selection_range, &text))
    return 0;
  DCHECK_EQ(selection_range.length(), text.length());

  reconv->dwVersion = 0;
  reconv->dwStrLen = len;
  reconv->dwStrOffset = sizeof(RECONVERTSTRING);
  reconv->dwCompStrLen = len;
  reconv->dwCompStrOffset = 0;
  reconv->dwTargetStrLen = len;
  reconv->dwTargetStrOffset = 0;

  memcpy(reinterpret_cast<char*>(reconv) + sizeof(RECONVERTSTRING),
         text.c_str(), len * sizeof(WCHAR));

  // According to Microsoft API document, IMR_RECONVERTSTRING and
  // IMR_DOCUMENTFEED should return reconv, but some applications return
  // need_size.
  return reinterpret_cast<LRESULT>(reconv);
}

LRESULT InputMethodWin::OnQueryCharPosition(IMECHARPOSITION* char_positon) {
  if (!char_positon)
    return 0;

  if (char_positon->dwSize < sizeof(IMECHARPOSITION))
    return 0;

  ui::TextInputClient* client = GetTextInputClient();
  if (!client)
    return 0;

  // Tentatively assume that the returned value from |client| is DIP (Density
  // Independent Pixel). See the comment in text_input_client.h and
  // http://crbug.com/360334.
  gfx::Rect dip_rect;
  if (client->HasCompositionText()) {
    if (!client->GetCompositionCharacterBounds(char_positon->dwCharPos,
                                               &dip_rect)) {
      return 0;
    }
  } else {
    // If there is no composition and the first character is queried, returns
    // the caret bounds. This behavior is the same to that of RichEdit control.
    if (char_positon->dwCharPos != 0)
      return 0;
    dip_rect = client->GetCaretBounds();
  }
  const gfx::Rect rect = gfx::win::DIPToScreenRect(dip_rect);

  char_positon->pt.x = rect.x();
  char_positon->pt.y = rect.y();
  char_positon->cLineHeight = rect.height();
  return 1;  // returns non-zero value when succeeded.
}

HWND InputMethodWin::GetAttachedWindowHandle(
  const TextInputClient* text_input_client) const {
  // On Aura environment, we can assume that |toplevel_window_handle_| always
  // represents the valid top-level window handle because each top-level window
  // is responsible for lifecycle management of corresponding InputMethod
  // instance.
  return toplevel_window_handle_;
}

bool InputMethodWin::IsWindowFocused(const TextInputClient* client) const {
  if (!client)
    return false;
  HWND attached_window_handle = GetAttachedWindowHandle(client);
  // When Aura is enabled, |attached_window_handle| should always be a top-level
  // window. So we can safely assume that |attached_window_handle| is ready for
  // receiving keyboard input as long as it is an active window. This works well
  // even when the |attached_window_handle| becomes active but has not received
  // WM_FOCUS yet.
  return attached_window_handle && GetActiveWindow() == attached_window_handle;
}

bool InputMethodWin::DispatchFabricatedKeyEvent(const ui::KeyEvent& event) {
  if (event.is_char()) {
    if (GetTextInputClient()) {
      GetTextInputClient()->InsertChar(event.key_code(),
                                       ui::GetModifiersFromKeyState());
      return true;
    }
  }
  return DispatchKeyEventPostIME(event);
}

void InputMethodWin::ConfirmCompositionText() {
  if (composing_window_handle_)
    imm32_manager_.CleanupComposition(composing_window_handle_);

  if (!IsTextInputTypeNone()) {
    // Though above line should confirm the client's composition text by sending
    // a result text to us, in case the input method and the client are in
    // inconsistent states, we check the client's composition state again.
    if (GetTextInputClient()->HasCompositionText())
      GetTextInputClient()->ConfirmCompositionText();
  }
}

void InputMethodWin::UpdateIMEState() {
  // Use switch here in case we are going to add more text input types.
  // We disable input method in password field.
  const HWND window_handle = GetAttachedWindowHandle(GetTextInputClient());
  const TextInputType text_input_type = GetTextInputType();
  const TextInputMode text_input_mode = GetTextInputMode();
  switch (text_input_type) {
    case ui::TEXT_INPUT_TYPE_NONE:
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      imm32_manager_.DisableIME(window_handle);
      enabled_ = false;
      break;
    default:
      imm32_manager_.EnableIME(window_handle);
      enabled_ = true;
      break;
  }

  imm32_manager_.SetTextInputMode(window_handle, text_input_mode);
  tsf_inputscope::SetInputScopeForTsfUnawareWindow(
      window_handle, text_input_type, text_input_mode);
}

}  // namespace ui
