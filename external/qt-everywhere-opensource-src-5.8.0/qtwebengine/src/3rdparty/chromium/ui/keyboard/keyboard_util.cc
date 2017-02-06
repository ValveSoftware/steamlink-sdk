// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_util.h"

#include <string>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string16.h"
#include "media/audio/audio_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_ui.h"

namespace {

const char kKeyDown[] ="keydown";
const char kKeyUp[] = "keyup";

void SendProcessKeyEvent(ui::EventType type,
                         aura::WindowTreeHost* host) {
  ui::KeyEvent event(type, ui::VKEY_PROCESSKEY, ui::DomCode::NONE,
                     ui::EF_IS_SYNTHESIZED, ui::DomKey::PROCESS,
                     ui::EventTimeForNow());
  ui::EventDispatchDetails details =
      host->event_processor()->OnEventFromSource(&event);
  CHECK(!details.dispatcher_destroyed);
}

base::LazyInstance<base::Time> g_keyboard_load_time_start =
    LAZY_INSTANCE_INITIALIZER;

bool g_accessibility_keyboard_enabled = false;

bool g_hotrod_keyboard_enabled = false;

bool g_touch_keyboard_enabled = false;

keyboard::KeyboardState g_requested_keyboard_state =
    keyboard::KEYBOARD_STATE_AUTO;

keyboard::KeyboardOverscrolOverride g_keyboard_overscroll_override =
    keyboard::KEYBOARD_OVERSCROLL_OVERRIDE_NONE;

keyboard::KeyboardShowOverride g_keyboard_show_override =
    keyboard::KEYBOARD_SHOW_OVERRIDE_NONE;

}  // namespace

namespace keyboard {

gfx::Rect FullWidthKeyboardBoundsFromRootBounds(const gfx::Rect& root_bounds,
                                                int keyboard_height) {
  return gfx::Rect(
      root_bounds.x(),
      root_bounds.bottom() - keyboard_height,
      root_bounds.width(),
      keyboard_height);
}

void SetAccessibilityKeyboardEnabled(bool enabled) {
  g_accessibility_keyboard_enabled = enabled;
}

bool GetAccessibilityKeyboardEnabled() {
  return g_accessibility_keyboard_enabled;
}

void SetHotrodKeyboardEnabled(bool enabled) {
  g_hotrod_keyboard_enabled = enabled;
}

bool GetHotrodKeyboardEnabled() {
  return g_hotrod_keyboard_enabled;
}

void SetTouchKeyboardEnabled(bool enabled) {
  g_touch_keyboard_enabled = enabled;
}

bool GetTouchKeyboardEnabled() {
  return g_touch_keyboard_enabled;
}

void SetRequestedKeyboardState(KeyboardState state) {
  g_requested_keyboard_state = state;
}

KeyboardState GetKeyboardRequestedState() {
  return g_requested_keyboard_state;
}

std::string GetKeyboardLayout() {
  // TODO(bshe): layout string is currently hard coded. We should use more
  // standard keyboard layouts.
  return GetAccessibilityKeyboardEnabled() ? "system-qwerty" : "qwerty";
}

bool IsKeyboardEnabled() {
  // Accessibility setting prioritized over policy setting.
  if (g_accessibility_keyboard_enabled)
    return true;
  // Policy strictly disables showing a virtual keyboard.
  if (g_keyboard_show_override == keyboard::KEYBOARD_SHOW_OVERRIDE_DISABLED)
    return false;
  // Policy strictly enables the keyboard.
  if (g_keyboard_show_override == keyboard::KEYBOARD_SHOW_OVERRIDE_ENABLED)
    return true;
  // Run-time flag to enable keyboard has been included.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableVirtualKeyboard))
    return true;
  // Requested state from the application layer.
  if (g_requested_keyboard_state == keyboard::KEYBOARD_STATE_DISABLED)
    return false;
  // Check if any of the other flags are enabled.
  return g_touch_keyboard_enabled ||
         g_requested_keyboard_state == keyboard::KEYBOARD_STATE_ENABLED;
}

bool IsKeyboardOverscrollEnabled() {
  if (!IsKeyboardEnabled())
    return false;

  // Users of the accessibility on-screen keyboard are likely to be using mouse
  // input, which may interfere with overscrolling.
  if (g_accessibility_keyboard_enabled)
    return false;

  // If overscroll enabled override is set, use it instead. Currently
  // login / out-of-box disable keyboard overscroll. http://crbug.com/363635
  if (g_keyboard_overscroll_override != KEYBOARD_OVERSCROLL_OVERRIDE_NONE) {
    return g_keyboard_overscroll_override ==
        KEYBOARD_OVERSCROLL_OVERRIDE_ENABLED;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableVirtualKeyboardOverscroll)) {
    return false;
  }
  return true;
}

void SetKeyboardOverscrollOverride(KeyboardOverscrolOverride override) {
  g_keyboard_overscroll_override = override;
}

void SetKeyboardShowOverride(KeyboardShowOverride override) {
  g_keyboard_show_override = override;
}

bool IsInputViewEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableInputView);
}

bool IsExperimentalInputViewEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalInputViewFeatures);
}

bool IsFloatingVirtualKeyboardEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableFloatingVirtualKeyboard);
}

bool IsGestureTypingEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableGestureTyping);
}

bool IsGestureEditingEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableGestureEditing);
}

bool IsSmartDeployEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSmartVirtualKeyboard);
}

bool IsVoiceInputEnabled() {
  return media::AudioManager::Get()->HasAudioInputDevices() &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableVoiceInput);
}

bool InsertText(const base::string16& text) {
  keyboard::KeyboardController* controller = KeyboardController::GetInstance();
  if (!controller)
    return false;

  ui::InputMethod* input_method = controller->ui()->GetInputMethod();
  if (!input_method)
    return false;

  ui::TextInputClient* tic = input_method->GetTextInputClient();
  if (!tic || tic->GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE)
    return false;

  tic->InsertText(text);

  return true;
}

// TODO(varunjain): It would be cleaner to have something in the
// ui::TextInputClient interface, say MoveCaretInDirection(). The code in
// here would get the ui::InputMethod from the root_window, and the
// ui::TextInputClient from that (see above in InsertText()).
bool MoveCursor(int swipe_direction,
                int modifier_flags,
                aura::WindowTreeHost* host) {
  if (!host)
    return false;
  ui::DomCode domcodex = ui::DomCode::NONE;
  ui::DomCode domcodey = ui::DomCode::NONE;
  if (swipe_direction & kCursorMoveRight)
    domcodex = ui::DomCode::ARROW_RIGHT;
  else if (swipe_direction & kCursorMoveLeft)
    domcodex = ui::DomCode::ARROW_LEFT;

  if (swipe_direction & kCursorMoveUp)
    domcodey = ui::DomCode::ARROW_UP;
  else if (swipe_direction & kCursorMoveDown)
    domcodey = ui::DomCode::ARROW_DOWN;

  // First deal with the x movement.
  if (domcodex != ui::DomCode::NONE) {
    ui::KeyboardCode codex = ui::VKEY_UNKNOWN;
    ui::DomKey domkeyx = ui::DomKey::NONE;
    ignore_result(DomCodeToUsLayoutDomKey(domcodex, ui::EF_NONE, &domkeyx,
                                          &codex));
    ui::KeyEvent press_event(ui::ET_KEY_PRESSED, codex, domcodex,
                             modifier_flags, domkeyx,
                             ui::EventTimeForNow());
    ui::EventDispatchDetails details =
        host->event_processor()->OnEventFromSource(&press_event);
    CHECK(!details.dispatcher_destroyed);
    ui::KeyEvent release_event(ui::ET_KEY_RELEASED, codex, domcodex,
                               modifier_flags, domkeyx,
                               ui::EventTimeForNow());
    details = host->event_processor()->OnEventFromSource(&release_event);
    CHECK(!details.dispatcher_destroyed);
  }

  // Then deal with the y movement.
  if (domcodey != ui::DomCode::NONE) {
    ui::KeyboardCode codey = ui::VKEY_UNKNOWN;
    ui::DomKey domkeyy = ui::DomKey::NONE;
    ignore_result(DomCodeToUsLayoutDomKey(domcodey, ui::EF_NONE, &domkeyy,
                                          &codey));
    ui::KeyEvent press_event(ui::ET_KEY_PRESSED, codey, domcodey,
                             modifier_flags, domkeyy,
                             ui::EventTimeForNow());
    ui::EventDispatchDetails details =
        host->event_processor()->OnEventFromSource(&press_event);
    CHECK(!details.dispatcher_destroyed);
    ui::KeyEvent release_event(ui::ET_KEY_RELEASED, codey, domcodey,
                               modifier_flags, domkeyy,
                               ui::EventTimeForNow());
    details = host->event_processor()->OnEventFromSource(&release_event);
    CHECK(!details.dispatcher_destroyed);
  }
  return true;
}

bool SendKeyEvent(const std::string type,
                  int key_value,
                  int key_code,
                  std::string key_name,
                  int modifiers,
                  aura::WindowTreeHost* host) {
  ui::EventType event_type = ui::ET_UNKNOWN;
  if (type == kKeyDown)
    event_type = ui::ET_KEY_PRESSED;
  else if (type == kKeyUp)
    event_type = ui::ET_KEY_RELEASED;
  if (event_type == ui::ET_UNKNOWN)
    return false;

  ui::KeyboardCode code = static_cast<ui::KeyboardCode>(key_code);

  ui::InputMethod* input_method = host->GetInputMethod();
  if (code == ui::VKEY_UNKNOWN) {
    // Handling of special printable characters (e.g. accented characters) for
    // which there is no key code.
    if (event_type == ui::ET_KEY_RELEASED) {
      if (!input_method)
        return false;

      ui::TextInputClient* tic = input_method->GetTextInputClient();

      SendProcessKeyEvent(ui::ET_KEY_PRESSED, host);

      ui::KeyEvent char_event(key_value, code, ui::EF_NONE);
      tic->InsertChar(char_event);
      SendProcessKeyEvent(ui::ET_KEY_RELEASED, host);
    }
  } else {
    if (event_type == ui::ET_KEY_RELEASED) {
      // The number of key press events seen since the last backspace.
      static int keys_seen = 0;
      if (code == ui::VKEY_BACK) {
        // Log the rough lengths of characters typed between backspaces. This
        // metric will be used to determine the error rate for the keyboard.
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "VirtualKeyboard.KeystrokesBetweenBackspaces",
            keys_seen, 1, 1000, 50);
        keys_seen = 0;
      } else {
        ++keys_seen;
      }
    }

    ui::DomCode dom_code = ui::KeycodeConverter::CodeStringToDomCode(key_name);
    if (dom_code == ui::DomCode::NONE)
      dom_code = ui::UsLayoutKeyboardCodeToDomCode(code);
    CHECK(dom_code != ui::DomCode::NONE);
    ui::KeyEvent event(
        event_type,
        code,
        dom_code,
        modifiers);
    if (input_method) {
      input_method->DispatchKeyEvent(&event);
    } else {
      ui::EventDispatchDetails details =
          host->event_processor()->OnEventFromSource(&event);
      CHECK(!details.dispatcher_destroyed);
    }
  }
  return true;
}

void MarkKeyboardLoadStarted() {
  if (!g_keyboard_load_time_start.Get().ToInternalValue())
    g_keyboard_load_time_start.Get() = base::Time::Now();
}

void MarkKeyboardLoadFinished() {
  // Possible to get a load finished without a start if navigating directly to
  // chrome://keyboard.
  if (!g_keyboard_load_time_start.Get().ToInternalValue())
    return;

  // It should not be possible to finish loading the keyboard without starting
  // to load it first.
  DCHECK(g_keyboard_load_time_start.Get().ToInternalValue());

  static bool logged = false;
  if (!logged) {
    // Log the delta only once.
    UMA_HISTOGRAM_TIMES(
        "VirtualKeyboard.FirstLoadTime",
        base::Time::Now() - g_keyboard_load_time_start.Get());
    logged = true;
  }
}

void LogKeyboardControlEvent(KeyboardControlEvent event) {
  UMA_HISTOGRAM_ENUMERATION(
      "VirtualKeyboard.KeyboardControlEvent",
      event,
      keyboard::KEYBOARD_CONTROL_MAX);
}

}  // namespace keyboard
