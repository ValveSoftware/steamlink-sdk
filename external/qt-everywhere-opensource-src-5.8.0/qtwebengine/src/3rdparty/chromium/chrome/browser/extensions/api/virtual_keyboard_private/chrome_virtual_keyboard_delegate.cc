// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/virtual_keyboard_private/chrome_virtual_keyboard_delegate.h"

#include <string>
#include <utility>

#include "ash/shell.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/common/api/virtual_keyboard_private.h"
#include "ui/aura/window_tree_host.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"

namespace keyboard_api = extensions::api::virtual_keyboard_private;

namespace {

aura::Window* GetKeyboardContainer() {
  keyboard::KeyboardController* controller =
      keyboard::KeyboardController::GetInstance();
  return controller ? controller->GetContainerWindow() : nullptr;
}

std::string GenerateFeatureFlag(const std::string& feature, bool enabled) {
  return feature + (enabled ? "-enabled" : "-disabled");
}

keyboard::KeyboardMode getKeyboardModeEnum(keyboard_api::KeyboardMode mode) {
  switch (mode) {
    case keyboard_api::KEYBOARD_MODE_NONE:
      return keyboard::NONE;
    case keyboard_api::KEYBOARD_MODE_FULL_WIDTH:
      return keyboard::FULL_WIDTH;
    case keyboard_api::KEYBOARD_MODE_FLOATING:
      return keyboard::FLOATING;
  }
  return keyboard::NONE;
}

keyboard::KeyboardState getKeyboardStateEnum(
    keyboard_api::KeyboardState state) {
  switch (state) {
    case keyboard_api::KEYBOARD_STATE_ENABLED:
      return keyboard::KEYBOARD_STATE_ENABLED;
    case keyboard_api::KEYBOARD_STATE_DISABLED:
      return keyboard::KEYBOARD_STATE_DISABLED;
    case keyboard_api::KEYBOARD_STATE_AUTO:
    case keyboard_api::KEYBOARD_STATE_NONE:
      return keyboard::KEYBOARD_STATE_AUTO;
  }
  return keyboard::KEYBOARD_STATE_AUTO;
}

}  // namespace

namespace extensions {

bool ChromeVirtualKeyboardDelegate::GetKeyboardConfig(
    base::DictionaryValue* results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  results->SetString("layout", keyboard::GetKeyboardLayout());
  // TODO(bshe): Consolidate a11y, hotrod and normal mode into a mode enum. See
  // crbug.com/529474.
  results->SetBoolean("a11ymode", keyboard::GetAccessibilityKeyboardEnabled());
  results->SetBoolean("hotrodmode", keyboard::GetHotrodKeyboardEnabled());
  std::unique_ptr<base::ListValue> features(new base::ListValue());
  features->AppendString(GenerateFeatureFlag(
      "floatingvirtualkeyboard", keyboard::IsFloatingVirtualKeyboardEnabled()));
  features->AppendString(
      GenerateFeatureFlag("gesturetyping", keyboard::IsGestureTypingEnabled()));
  features->AppendString(GenerateFeatureFlag(
      "gestureediting", keyboard::IsGestureEditingEnabled()));
  features->AppendString(
      GenerateFeatureFlag("voiceinput", keyboard::IsVoiceInputEnabled()));
  features->AppendString(GenerateFeatureFlag("experimental",
      keyboard::IsExperimentalInputViewEnabled()));
  results->Set("features", std::move(features));
  return true;
}

bool ChromeVirtualKeyboardDelegate::HideKeyboard() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  keyboard::KeyboardController* controller =
      keyboard::KeyboardController::GetInstance();
  if (!controller)
    return false;

  UMA_HISTOGRAM_ENUMERATION("VirtualKeyboard.KeyboardControlEvent",
                            keyboard::KEYBOARD_CONTROL_HIDE_USER,
                            keyboard::KEYBOARD_CONTROL_MAX);

  // Pass HIDE_REASON_MANUAL since calls to HideKeyboard as part of this API
  // would be user generated.
  controller->HideKeyboard(keyboard::KeyboardController::HIDE_REASON_MANUAL);
  return true;
}

bool ChromeVirtualKeyboardDelegate::InsertText(const base::string16& text) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return keyboard::InsertText(text);
}

bool ChromeVirtualKeyboardDelegate::OnKeyboardLoaded() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  keyboard::MarkKeyboardLoadFinished();
  base::UserMetricsAction("VirtualKeyboardLoaded");
  return true;
}

void ChromeVirtualKeyboardDelegate::SetHotrodKeyboard(bool enable) {
  if (keyboard::GetHotrodKeyboardEnabled() == enable)
    return;

  keyboard::SetHotrodKeyboardEnabled(enable);
  // This reloads virtual keyboard even if it exists. This ensures virtual
  // keyboard gets the correct state of the hotrod keyboard through
  // chrome.virtualKeyboardPrivate.getKeyboardConfig.
  if (keyboard::IsKeyboardEnabled())
    ash::Shell::GetInstance()->CreateKeyboard();
}

bool ChromeVirtualKeyboardDelegate::LockKeyboard(bool state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  keyboard::KeyboardController* controller =
      keyboard::KeyboardController::GetInstance();
  if (!controller)
    return false;

  keyboard::KeyboardController::GetInstance()->set_lock_keyboard(state);
  return true;
}

bool ChromeVirtualKeyboardDelegate::SendKeyEvent(const std::string& type,
                                                 int char_value,
                                                 int key_code,
                                                 const std::string& key_name,
                                                 int modifiers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  aura::Window* window = GetKeyboardContainer();
  return window && keyboard::SendKeyEvent(type,
                                          char_value,
                                          key_code,
                                          key_name,
                                          modifiers,
                                          window->GetHost());
}

bool ChromeVirtualKeyboardDelegate::ShowLanguageSettings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RecordAction(base::UserMetricsAction("OpenLanguageOptionsDialog"));
  chrome::ShowSettingsSubPageForProfile(ProfileManager::GetActiveUserProfile(),
                                        chrome::kLanguageOptionsSubPage);
  return true;
}

bool ChromeVirtualKeyboardDelegate::SetVirtualKeyboardMode(int mode_enum) {
  keyboard::KeyboardMode keyboard_mode =
      getKeyboardModeEnum(static_cast<keyboard_api::KeyboardMode>(mode_enum));
  keyboard::KeyboardController* controller =
      keyboard::KeyboardController::GetInstance();
  if (!controller)
    return false;

  controller->SetKeyboardMode(keyboard_mode);
  return true;
}

bool ChromeVirtualKeyboardDelegate::SetRequestedKeyboardState(int state_enum) {
  keyboard::KeyboardState keyboard_state = getKeyboardStateEnum(
      static_cast<keyboard_api::KeyboardState>(state_enum));
  bool was_enabled = keyboard::IsKeyboardEnabled();
  keyboard::SetRequestedKeyboardState(keyboard_state);
  bool is_enabled = keyboard::IsKeyboardEnabled();
  if (was_enabled == is_enabled)
    return true;
  if (is_enabled) {
    ash::Shell::GetInstance()->CreateKeyboard();
  } else {
    ash::Shell::GetInstance()->DeactivateKeyboard();
  }
  return true;
}

bool ChromeVirtualKeyboardDelegate::IsLanguageSettingsEnabled() {
  return (user_manager::UserManager::Get()->IsUserLoggedIn() &&
          !chromeos::UserAddingScreen::Get()->IsRunning() &&
          !(chromeos::ScreenLocker::default_screen_locker() &&
            chromeos::ScreenLocker::default_screen_locker()->locked()));
}

}  // namespace extensions
