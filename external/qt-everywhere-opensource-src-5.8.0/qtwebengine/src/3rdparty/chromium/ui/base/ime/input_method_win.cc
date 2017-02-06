// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_win.h"

#include <stddef.h>
#include <stdint.h>
#include <cwctype>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/ime_engine_handler_interface.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/win/tsf_input_scope.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/win/screen_win.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/win/hwnd_util.h"

namespace {

ui::IMEEngineHandlerInterface* GetEngine() {
  if (ui::IMEBridge::Get())
    return ui::IMEBridge::Get()->GetCurrentEngineHandler();
  return nullptr;
}

}  // namespace

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
      enabled_(false),
      is_candidate_popup_open_(false),
      composing_window_handle_(NULL),
      weak_ptr_factory_(this) {
  SetDelegate(delegate);
}

InputMethodWin::~InputMethodWin() {}

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
      original_result = OnChar(event.hwnd, event.message, event.wParam,
                               event.lParam, event, &handled);
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

void InputMethodWin::DispatchKeyEvent(ui::KeyEvent* event) {
  if (!event->HasNativeEvent()) {
    DispatchFabricatedKeyEvent(event);
    return;
  }

  const base::NativeEvent& native_key_event = event->native_event();
  BOOL handled = FALSE;
  if (native_key_event.message == WM_CHAR) {
    OnChar(native_key_event.hwnd, native_key_event.message,
           native_key_event.wParam, native_key_event.lParam, native_key_event,
           &handled);
    if (handled)
      event->StopPropagation();
    return;
  }

  std::vector<MSG> char_msgs;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableMergeKeyCharEvents)) {
    // Combines the WM_KEY* and WM_CHAR messages in the event processing flow
    // which is necessary to let Chrome IME extension to process the key event
    // and perform corresponding IME actions.
    // Chrome IME extension may wants to consume certain key events based on
    // the character information of WM_CHAR messages. Holding WM_KEY* messages
    // until WM_CHAR is processed by the IME extension is not feasible because
    // there is no way to know wether there will or not be a WM_CHAR following
    // the WM_KEY*.
    // Chrome never handles dead chars so it is safe to remove/ignore
    // WM_*DEADCHAR messages.
    MSG msg;
    while (::PeekMessage(&msg, native_key_event.hwnd, WM_CHAR, WM_DEADCHAR,
                         PM_REMOVE)) {
      if (msg.message == WM_CHAR)
        char_msgs.push_back(msg);
    }
    while (::PeekMessage(&msg, native_key_event.hwnd, WM_SYSCHAR,
                         WM_SYSDEADCHAR, PM_REMOVE)) {
      if (msg.message == WM_SYSCHAR)
        char_msgs.push_back(msg);
    }
  }

  // Handles ctrl-shift key to change text direction and layout alignment.
  if (ui::IMM32Manager::IsRTLKeyboardLayoutInstalled() &&
      !IsTextInputTypeNone()) {
    // TODO: shouldn't need to generate a KeyEvent here.
    const ui::KeyEvent key(native_key_event);
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

  // If only 1 WM_CHAR per the key event, set it as the character of it.
  if (char_msgs.size() == 1 &&
      !std::iswcntrl(static_cast<wint_t>(char_msgs[0].wParam)))
    event->set_character(static_cast<base::char16>(char_msgs[0].wParam));

  // Dispatches the key events to the Chrome IME extension which is listening to
  // key events on the following two situations:
  // 1) |char_msgs| is empty when the event is non-character key.
  // 2) |char_msgs|.size() == 1 when the event is character key and the WM_CHAR
  // messages have been combined in the event processing flow.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableMergeKeyCharEvents) &&
      char_msgs.size() <= 1 && GetEngine() &&
      GetEngine()->IsInterestedInKeyEvent()) {
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback = base::Bind(
        &InputMethodWin::ProcessKeyEventDone, weak_ptr_factory_.GetWeakPtr(),
        base::Owned(new ui::KeyEvent(*event)),
        base::Owned(new std::vector<MSG>(char_msgs)));
    GetEngine()->ProcessKeyEvent(*event, callback);
  } else {
    ProcessKeyEventDone(event, &char_msgs, false);
  }
}

void InputMethodWin::ProcessKeyEventDone(ui::KeyEvent* event,
                                         const std::vector<MSG>* char_msgs,
                                         bool is_handled) {
  DCHECK(event);
  if (is_handled)
    return;

  ui::EventDispatchDetails details = DispatchKeyEventPostIME(event);
  if (details.dispatcher_destroyed || details.target_destroyed ||
      event->stopped_propagation()) {
    return;
  }

  BOOL handled;
  for (const auto& msg : (*char_msgs))
    OnChar(msg.hwnd, msg.message, msg.wParam, msg.lParam, msg, &handled);
}

void InputMethodWin::OnTextInputTypeChanged(const TextInputClient* client) {
  if (!IsTextInputClientFocused(client) || !IsWindowFocused(client))
    return;
  imm32_manager_.CancelIME(toplevel_window_handle_);
  UpdateIMEState();
}

void InputMethodWin::OnCaretBoundsChanged(const TextInputClient* client) {
  if (!IsTextInputClientFocused(client) || !IsWindowFocused(client))
    return;
  TextInputType text_input_type = GetTextInputType();
  if (client == GetTextInputClient() &&
      text_input_type != TEXT_INPUT_TYPE_NONE &&
      text_input_type != TEXT_INPUT_TYPE_PASSWORD && GetEngine()) {
    // |enabled_| == false could be faked, and the engine should rely on the
    // real type from GetTextInputType().
    GetEngine()->SetCompositionBounds(GetCompositionBounds(client));
  }
  if (!enabled_)
    return;

  // The current text input type should not be NONE if |client| is focused.
  DCHECK(!IsTextInputTypeNone());
  // Tentatively assume that the returned value is DIP (Density Independent
  // Pixel). See the comment in text_input_client.h and http://crbug.com/360334.
  const gfx::Rect dip_screen_bounds(GetTextInputClient()->GetCaretBounds());
  const gfx::Rect screen_bounds =
      display::win::ScreenWin::DIPToScreenRect(toplevel_window_handle_,
                                               dip_screen_bounds);

  HWND attached_window = toplevel_window_handle_;
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
  if (IsTextInputClientFocused(client)) {
    // |enabled_| == false could be faked, and the engine should rely on the
    // real type get from GetTextInputType().
    TextInputType text_input_type = GetTextInputType();
    if (text_input_type != TEXT_INPUT_TYPE_NONE &&
        text_input_type != TEXT_INPUT_TYPE_PASSWORD && GetEngine()) {
      GetEngine()->Reset();
    }

    if (enabled_)
      imm32_manager_.CancelIME(toplevel_window_handle_);
  }
}

void InputMethodWin::OnInputLocaleChanged() {
  // Note: OnInputLocaleChanged() is for crbug.com/168971.
  OnInputMethodChanged();
}

std::string InputMethodWin::GetInputLocale() {
  return imm32_manager_.GetInputLanguageName();
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
                               const base::NativeEvent& event,
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
    const uint32_t kPrevKeyDownBit = 0x40000000;
    if (ch == kCarriageReturn && !(lparam & kPrevKeyDownBit))
      accept_carriage_return_ = true;
    // Conditionally ignore '\r' events to work around crbug.com/319100.
    // TODO(yukawa, IME): Figure out long-term solution.
    if (ch != kCarriageReturn || accept_carriage_return_) {
      ui::KeyEvent char_event(event);
      GetTextInputClient()->InsertChar(char_event);
    }
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
  if (!!wparam) {
    imm32_manager_.CreateImeWindow(window_handle);
    // Delay initialize the tsf to avoid perf regression.
    // Loading tsf dll causes some time, so doing it in UpdateIMEState() will
    // slow down the browser window creation.
    // See crbug.com/509984.
    tsf_inputscope::InitializeTsfForInputScopes();
    tsf_inputscope::SetInputScopeForTsfUnawareWindow(
        toplevel_window_handle_, GetTextInputType(), GetTextInputMode());
  }

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

  // Update |is_candidate_popup_open_|, whether a candidate window is open.
  switch (wparam) {
  case IMN_OPENCANDIDATE:
    is_candidate_popup_open_ = true;
    break;
  case IMN_CLOSECANDIDATE:
    is_candidate_popup_open_ = false;
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
  const gfx::Rect rect =
      display::win::ScreenWin::DIPToScreenRect(toplevel_window_handle_,
                                               dip_rect);

  char_positon->pt.x = rect.x();
  char_positon->pt.y = rect.y();
  char_positon->cLineHeight = rect.height();
  return 1;  // returns non-zero value when succeeded.
}

bool InputMethodWin::IsWindowFocused(const TextInputClient* client) const {
  if (!client)
    return false;
  // When Aura is enabled, |attached_window_handle| should always be a top-level
  // window. So we can safely assume that |attached_window_handle| is ready for
  // receiving keyboard input as long as it is an active window. This works well
  // even when the |attached_window_handle| becomes active but has not received
  // WM_FOCUS yet.
  return toplevel_window_handle_ &&
      GetActiveWindow() == toplevel_window_handle_;
}

void InputMethodWin::DispatchFabricatedKeyEvent(ui::KeyEvent* event) {
  // The key event if from calling input.ime.sendKeyEvent or test.
  ui::EventDispatchDetails details = DispatchKeyEventPostIME(event);
  if (details.dispatcher_destroyed || details.target_destroyed ||
      event->stopped_propagation()) {
    return;
  }

  if ((event->is_char() || event->GetDomKey().IsCharacter()) &&
      event->type() == ui::ET_KEY_PRESSED && GetTextInputClient())
    GetTextInputClient()->InsertChar(*event);
}

void InputMethodWin::ConfirmCompositionText() {
  if (composing_window_handle_)
    imm32_manager_.CleanupComposition(composing_window_handle_);

  // Though above line should confirm the client's composition text by sending a
  // result text to us, in case the input method and the client are in
  // inconsistent states, we check the client's composition state again.
  if (!IsTextInputTypeNone() && GetTextInputClient()->HasCompositionText()) {
    GetTextInputClient()->ConfirmCompositionText();

    if (GetEngine())
      GetEngine()->Reset();
  }
}

void InputMethodWin::UpdateIMEState() {
  // Use switch here in case we are going to add more text input types.
  // We disable input method in password field.
  const HWND window_handle = toplevel_window_handle_;
  const TextInputType text_input_type =
      (GetEngine() && GetEngine()->IsInterestedInKeyEvent())
          ? TEXT_INPUT_TYPE_NONE
          : GetTextInputType();
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

  if (!ui::IMEBridge::Get())  // IMEBridge could be null for tests.
    return;

  const TextInputType old_text_input_type =
      ui::IMEBridge::Get()->GetCurrentInputContext().type;
  ui::IMEEngineHandlerInterface::InputContext context(
      GetTextInputType(), GetTextInputMode(), GetTextInputFlags());
  ui::IMEBridge::Get()->SetCurrentInputContext(context);

  ui::IMEEngineHandlerInterface* engine = GetEngine();
  if (engine) {
    if (old_text_input_type != ui::TEXT_INPUT_TYPE_NONE)
      engine->FocusOut();
    if (GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE)
      engine->FocusIn(context);
  }
}

}  // namespace ui
