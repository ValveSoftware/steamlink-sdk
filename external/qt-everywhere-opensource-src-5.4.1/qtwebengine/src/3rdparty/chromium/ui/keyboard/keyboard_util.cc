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
#include "grit/keyboard_resources.h"
#include "grit/keyboard_resources_map.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event_processor.h"
#include "ui/keyboard/keyboard_switches.h"
#include "url/gurl.h"

namespace {

const char kKeyDown[] ="keydown";
const char kKeyUp[] = "keyup";

void SendProcessKeyEvent(ui::EventType type,
                         aura::WindowTreeHost* host) {
  ui::KeyEvent event(type, ui::VKEY_PROCESSKEY, ui::EF_NONE, false);
  event.SetTranslated(true);
  ui::EventDispatchDetails details =
      host->event_processor()->OnEventFromSource(&event);
  CHECK(!details.dispatcher_destroyed);
}

base::LazyInstance<base::Time> g_keyboard_load_time_start =
    LAZY_INSTANCE_INITIALIZER;

bool g_accessibility_keyboard_enabled = false;

base::LazyInstance<GURL> g_override_content_url = LAZY_INSTANCE_INITIALIZER;

bool g_touch_keyboard_enabled = false;

keyboard::KeyboardOverscrolOverride g_keyboard_overscroll_override =
    keyboard::KEYBOARD_OVERSCROLL_OVERRIDE_NONE;

keyboard::KeyboardShowOverride g_keyboard_show_override =
    keyboard::KEYBOARD_SHOW_OVERRIDE_NONE;

}  // namespace

namespace keyboard {

gfx::Rect DefaultKeyboardBoundsFromWindowBounds(
    const gfx::Rect& window_bounds) {
  // Initialize default keyboard height to 0. The keyboard window height should
  // only be set by window.resizeTo in virtual keyboard web contents. Otherwise,
  // the default height may conflict with the new height and causing some
  // strange animation issues. For keyboard usability experiments, a full screen
  // virtual keyboard window is always preferred.
  int keyboard_height =
      keyboard::IsKeyboardUsabilityExperimentEnabled() ?
          window_bounds.height() : 0;

  return KeyboardBoundsFromWindowBounds(window_bounds, keyboard_height);
}

gfx::Rect KeyboardBoundsFromWindowBounds(const gfx::Rect& window_bounds,
                                         int keyboard_height) {
  return gfx::Rect(
      window_bounds.x(),
      window_bounds.bottom() - keyboard_height,
      window_bounds.width(),
      keyboard_height);
}

void SetAccessibilityKeyboardEnabled(bool enabled) {
  g_accessibility_keyboard_enabled = enabled;
}

bool GetAccessibilityKeyboardEnabled() {
  return g_accessibility_keyboard_enabled;
}

void SetTouchKeyboardEnabled(bool enabled) {
  g_touch_keyboard_enabled = enabled;
}

bool GetTouchKeyboardEnabled() {
  return g_touch_keyboard_enabled;
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
  // Check if any of the flags are enabled.
  return CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableVirtualKeyboard) ||
         IsKeyboardUsabilityExperimentEnabled() ||
         g_touch_keyboard_enabled ||
         (g_keyboard_show_override == keyboard::KEYBOARD_SHOW_OVERRIDE_ENABLED);
}

bool IsKeyboardUsabilityExperimentEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kKeyboardUsabilityExperiment);
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

  if (CommandLine::ForCurrentProcess()->HasSwitch(
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
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableInputView))
    return true;
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableInputView))
    return false;
  // Default value if no command line flags specified.
  return true;
}

bool IsExperimentalInputViewEnabled() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableExperimentalInputViewFeatures)) {
    return true;
  }
  return false;
}

bool InsertText(const base::string16& text, aura::Window* root_window) {
  if (!root_window)
    return false;

  ui::InputMethod* input_method = root_window->GetProperty(
      aura::client::kRootWindowInputMethodKey);
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
  ui::KeyboardCode codex = ui::VKEY_UNKNOWN;
  ui::KeyboardCode codey = ui::VKEY_UNKNOWN;
  if (swipe_direction & kCursorMoveRight)
    codex = ui::VKEY_RIGHT;
  else if (swipe_direction & kCursorMoveLeft)
    codex = ui::VKEY_LEFT;

  if (swipe_direction & kCursorMoveUp)
    codey = ui::VKEY_UP;
  else if (swipe_direction & kCursorMoveDown)
    codey = ui::VKEY_DOWN;

  // First deal with the x movement.
  if (codex != ui::VKEY_UNKNOWN) {
    ui::KeyEvent press_event(ui::ET_KEY_PRESSED, codex, modifier_flags, 0);
    ui::EventDispatchDetails details =
        host->event_processor()->OnEventFromSource(&press_event);
    CHECK(!details.dispatcher_destroyed);
    ui::KeyEvent release_event(ui::ET_KEY_RELEASED, codex, modifier_flags, 0);
    details = host->event_processor()->OnEventFromSource(&release_event);
    CHECK(!details.dispatcher_destroyed);
  }

  // Then deal with the y movement.
  if (codey != ui::VKEY_UNKNOWN) {
    ui::KeyEvent press_event(ui::ET_KEY_PRESSED, codey, modifier_flags, 0);
    ui::EventDispatchDetails details =
        host->event_processor()->OnEventFromSource(&press_event);
    CHECK(!details.dispatcher_destroyed);
    ui::KeyEvent release_event(ui::ET_KEY_RELEASED, codey, modifier_flags, 0);
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

  if (code == ui::VKEY_UNKNOWN) {
    // Handling of special printable characters (e.g. accented characters) for
    // which there is no key code.
    if (event_type == ui::ET_KEY_RELEASED) {
      ui::InputMethod* input_method = host->window()->GetProperty(
          aura::client::kRootWindowInputMethodKey);
      if (!input_method)
        return false;

      ui::TextInputClient* tic = input_method->GetTextInputClient();

      SendProcessKeyEvent(ui::ET_KEY_PRESSED, host);
      tic->InsertChar(static_cast<uint16>(key_value), ui::EF_NONE);
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

    ui::KeyEvent event(event_type, code, key_name, modifiers, false);
    ui::EventDispatchDetails details =
        host->event_processor()->OnEventFromSource(&event);
    CHECK(!details.dispatcher_destroyed);
  }
  return true;
}

const void MarkKeyboardLoadStarted() {
  if (!g_keyboard_load_time_start.Get().ToInternalValue())
    g_keyboard_load_time_start.Get() = base::Time::Now();
}

const void MarkKeyboardLoadFinished() {
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

const GritResourceMap* GetKeyboardExtensionResources(size_t* size) {
  // This looks a lot like the contents of a resource map; however it is
  // necessary to have a custom path for the extension path, so the resource
  // map cannot be used directly.
  static const GritResourceMap kKeyboardResources[] = {
    {"keyboard/layouts/function-key-row.html", IDR_KEYBOARD_FUNCTION_KEY_ROW},
    {"keyboard/images/back.svg", IDR_KEYBOARD_IMAGES_BACK},
    {"keyboard/images/backspace.png", IDR_KEYBOARD_IMAGES_BACKSPACE},
    {"keyboard/images/brightness-down.svg",
        IDR_KEYBOARD_IMAGES_BRIGHTNESS_DOWN},
    {"keyboard/images/brightness-up.svg", IDR_KEYBOARD_IMAGES_BRIGHTNESS_UP},
    {"keyboard/images/change-window.svg", IDR_KEYBOARD_IMAGES_CHANGE_WINDOW},
    {"keyboard/images/down.svg", IDR_KEYBOARD_IMAGES_DOWN},
    {"keyboard/images/forward.svg", IDR_KEYBOARD_IMAGES_FORWARD},
    {"keyboard/images/fullscreen.svg", IDR_KEYBOARD_IMAGES_FULLSCREEN},
    {"keyboard/images/hide-keyboard.png", IDR_KEYBOARD_IMAGES_HIDE_KEYBOARD},
    {"keyboard/images/keyboard.svg", IDR_KEYBOARD_IMAGES_KEYBOARD},
    {"keyboard/images/left.svg", IDR_KEYBOARD_IMAGES_LEFT},
    {"keyboard/images/microphone.svg", IDR_KEYBOARD_IMAGES_MICROPHONE},
    {"keyboard/images/microphone-green.svg",
        IDR_KEYBOARD_IMAGES_MICROPHONE_GREEN},
    {"keyboard/images/mute.svg", IDR_KEYBOARD_IMAGES_MUTE},
    {"keyboard/images/reload.svg", IDR_KEYBOARD_IMAGES_RELOAD},
    {"keyboard/images/return.png", IDR_KEYBOARD_IMAGES_RETURN},
    {"keyboard/images/right.svg", IDR_KEYBOARD_IMAGES_RIGHT},
    {"keyboard/images/search.png", IDR_KEYBOARD_IMAGES_SEARCH},
    {"keyboard/images/shift.png", IDR_KEYBOARD_IMAGES_SHIFT},
    {"keyboard/images/shutdown.svg", IDR_KEYBOARD_IMAGES_SHUTDOWN},
    {"keyboard/images/tab.png", IDR_KEYBOARD_IMAGES_TAB},
    {"keyboard/images/up.svg", IDR_KEYBOARD_IMAGES_UP},
    {"keyboard/images/volume-down.svg", IDR_KEYBOARD_IMAGES_VOLUME_DOWN},
    {"keyboard/images/volume-up.svg", IDR_KEYBOARD_IMAGES_VOLUME_UP},
    {"keyboard/index.html", IDR_KEYBOARD_INDEX},
    {"keyboard/keyboard.js", IDR_KEYBOARD_JS},
    {"keyboard/layouts/numeric.html", IDR_KEYBOARD_LAYOUTS_NUMERIC},
    {"keyboard/layouts/qwerty.html", IDR_KEYBOARD_LAYOUTS_QWERTY},
    {"keyboard/layouts/system-qwerty.html", IDR_KEYBOARD_LAYOUTS_SYSTEM_QWERTY},
    {"keyboard/layouts/spacebar-row.html", IDR_KEYBOARD_SPACEBAR_ROW},
    {"keyboard/manifest.json", IDR_KEYBOARD_MANIFEST},
    {"keyboard/main.css", IDR_KEYBOARD_MAIN_CSS},
    {"keyboard/polymer_loader.js", IDR_KEYBOARD_POLYMER_LOADER},
    {"keyboard/roboto_bold.ttf", IDR_KEYBOARD_ROBOTO_BOLD_TTF},
    {"keyboard/sounds/keypress-delete.wav",
        IDR_KEYBOARD_SOUNDS_KEYPRESS_DELETE},
    {"keyboard/sounds/keypress-return.wav",
        IDR_KEYBOARD_SOUNDS_KEYPRESS_RETURN},
    {"keyboard/sounds/keypress-spacebar.wav",
        IDR_KEYBOARD_SOUNDS_KEYPRESS_SPACEBAR},
    {"keyboard/sounds/keypress-standard.wav",
        IDR_KEYBOARD_SOUNDS_KEYPRESS_STANDARD},
  };
  static const size_t kKeyboardResourcesSize = arraysize(kKeyboardResources);
  *size = kKeyboardResourcesSize;
  return kKeyboardResources;
}

void SetOverrideContentUrl(const GURL& url) {
  g_override_content_url.Get() = url;
}

const GURL& GetOverrideContentUrl() {
  return g_override_content_url.Get();
}

void LogKeyboardControlEvent(KeyboardControlEvent event) {
  UMA_HISTOGRAM_ENUMERATION(
      "VirtualKeyboard.KeyboardControlEvent",
      event,
      keyboard::KEYBOARD_CONTROL_MAX);
}

}  // namespace keyboard
