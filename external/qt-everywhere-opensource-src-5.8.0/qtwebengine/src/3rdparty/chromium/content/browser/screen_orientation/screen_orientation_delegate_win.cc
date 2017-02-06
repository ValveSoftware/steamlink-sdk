// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_orientation/screen_orientation_delegate_win.h"

#include <windows.h>

#include "content/public/browser/screen_orientation_provider.h"

namespace {

// SetDisplayAutoRotationPreferences is available on Windows 8 and after.
void SetDisplayAutoRotationPreferencesWrapper(
    ORIENTATION_PREFERENCE orientation) {
  using SetDisplayAutoRotationPreferencesPtr =
      void(WINAPI*)(ORIENTATION_PREFERENCE);
  static SetDisplayAutoRotationPreferencesPtr
      set_display_auto_rotation_preferences_func =
          reinterpret_cast<SetDisplayAutoRotationPreferencesPtr>(
              GetProcAddress(GetModuleHandleA("user32.dll"),
                             "SetDisplayAutoRotationPreferences"));
  if (set_display_auto_rotation_preferences_func)
    set_display_auto_rotation_preferences_func(orientation);
}

// GetAutoRotationState is available on Windows 8 and after.
BOOL GetAutoRotationStateWrapper(PAR_STATE state) {
  using GetAutoRotationStatePtr = BOOL(WINAPI*)(PAR_STATE);
  static GetAutoRotationStatePtr get_auto_rotation_state_func =
      reinterpret_cast<GetAutoRotationStatePtr>(GetProcAddress(
          GetModuleHandleA("user32.dll"), "GetAutoRotationState"));
  if (get_auto_rotation_state_func)
    return get_auto_rotation_state_func(state);
  return FALSE;
}

bool GetDisplayOrientation(bool* landscape, bool* flipped) {
  DEVMODE dm = {};
  dm.dmSize = sizeof(dm);
  if (!EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &dm))
    return false;
  *flipped = (dm.dmDisplayOrientation == DMDO_270 ||
              dm.dmDisplayOrientation == DMDO_180);
  *landscape = (dm.dmPelsWidth > dm.dmPelsHeight);
  return true;
}

}  // namespace

namespace content {

ScreenOrientationDelegateWin::ScreenOrientationDelegateWin() {
  ScreenOrientationProvider::SetDelegate(this);
}

ScreenOrientationDelegateWin::~ScreenOrientationDelegateWin() {
  ScreenOrientationProvider::SetDelegate(nullptr);
}

bool ScreenOrientationDelegateWin::FullScreenRequired(
    WebContents* web_contents) {
  return false;
}

void ScreenOrientationDelegateWin::Lock(
    WebContents* web_contents,
    blink::WebScreenOrientationLockType lock_orientation) {
  ORIENTATION_PREFERENCE prefs = ORIENTATION_PREFERENCE_NONE;
  bool landscape = true;
  bool flipped = false;
  switch (lock_orientation) {
    case blink::WebScreenOrientationLockPortraitPrimary:
      prefs = ORIENTATION_PREFERENCE_PORTRAIT;
      break;
    case blink::WebScreenOrientationLockPortraitSecondary:
      prefs = ORIENTATION_PREFERENCE_PORTRAIT_FLIPPED;
      break;
    case blink::WebScreenOrientationLockLandscapePrimary:
      prefs = ORIENTATION_PREFERENCE_LANDSCAPE;
      break;
    case blink::WebScreenOrientationLockLandscapeSecondary:
      prefs = ORIENTATION_PREFERENCE_LANDSCAPE_FLIPPED;
      break;
    case blink::WebScreenOrientationLockPortrait:
      if (!GetDisplayOrientation(&landscape, &flipped))
        return;
      prefs = (flipped && !landscape) ? ORIENTATION_PREFERENCE_PORTRAIT_FLIPPED
                                      : ORIENTATION_PREFERENCE_PORTRAIT;
      break;
    case blink::WebScreenOrientationLockLandscape:
      if (!GetDisplayOrientation(&landscape, &flipped))
        return;
      prefs = (flipped && landscape) ? ORIENTATION_PREFERENCE_LANDSCAPE_FLIPPED
                                     : ORIENTATION_PREFERENCE_LANDSCAPE;
      break;
    case blink::WebScreenOrientationLockNatural:
      if (!GetDisplayOrientation(&landscape, &flipped))
        return;
      prefs = landscape ? ORIENTATION_PREFERENCE_LANDSCAPE
                        : ORIENTATION_PREFERENCE_PORTRAIT;
      break;
    case blink::WebScreenOrientationLockAny:
      if (!GetDisplayOrientation(&landscape, &flipped))
        return;
      if (landscape) {
        prefs = flipped ? ORIENTATION_PREFERENCE_LANDSCAPE_FLIPPED
                        : ORIENTATION_PREFERENCE_LANDSCAPE;
      } else {
        prefs = flipped ? ORIENTATION_PREFERENCE_PORTRAIT_FLIPPED
                        : ORIENTATION_PREFERENCE_PORTRAIT;
      }
      break;
    case blink::WebScreenOrientationLockDefault:
    default:
      break;
  }
  SetDisplayAutoRotationPreferencesWrapper(prefs);
}

bool ScreenOrientationDelegateWin::ScreenOrientationProviderSupported() {
  AR_STATE auto_rotation_state = {};
  return (GetAutoRotationStateWrapper(&auto_rotation_state) &&
          !(auto_rotation_state & AR_NOSENSOR) &&
          !(auto_rotation_state & AR_NOT_SUPPORTED) &&
          !(auto_rotation_state & AR_MULTIMON));
}

void ScreenOrientationDelegateWin::Unlock(WebContents* web_contents) {
  SetDisplayAutoRotationPreferencesWrapper(ORIENTATION_PREFERENCE_NONE);
}

}  // namespace content
