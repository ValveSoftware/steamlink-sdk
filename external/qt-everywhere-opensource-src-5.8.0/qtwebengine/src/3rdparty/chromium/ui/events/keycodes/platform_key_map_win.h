// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_PLATFORM_KEY_MAP_WIN_H_
#define UI_EVENTS_KEYCODES_PLATFORM_KEY_MAP_WIN_H_

#include <windows.h>

#include <unordered_map>

#include "base/event_types.h"
#include "base/hash.h"
#include "ui/events/events_export.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_win.h"

namespace ui {

class EVENTS_EXPORT PlatformKeyMap {
 public:
  // Create and load key map table with specified keyboard layout.
  // Visible for testing.
  explicit PlatformKeyMap(HKL layout);
  ~PlatformKeyMap();

  // Returns the DOM KeyboardEvent key from |KeyboardCode|+|EventFlags| and
  // the keyboard layout of current thread.
  // Updates a per-thread key map cache whenever the layout changes.
  static DomKey DomKeyFromKeyboardCode(KeyboardCode, int flags);

 private:
  friend class PlatformKeyMapTest;

  PlatformKeyMap();

  // TODO(chongz): Expose this function when we need to access separate layout.
  // Returns the DomKey 'meaning' of |KeyboardCode| in the context of specified
  // |EventFlags| and stored keyboard layout.
  DomKey DomKeyFromKeyboardCodeImpl(KeyboardCode, int flags) const;

  // TODO(chongz): Expose this function in response to WM_INPUTLANGCHANGE.
  void UpdateLayout(HKL layout);

  HKL keyboard_layout_ = 0;

  typedef std::pair<int /*KeyboardCode*/, int /*EventFlags*/>
      KeyboardCodeEventFlagsPair;
  typedef std::unordered_map<KeyboardCodeEventFlagsPair,
                             DomKey,
                             base::IntPairHash<std::pair<int, int>>>
      KeyboardCodeToKeyMap;
  KeyboardCodeToKeyMap printable_keycode_to_key_;

  DISALLOW_COPY_AND_ASSIGN(PlatformKeyMap);
};

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_PLATFORM_KEY_MAP_WIN_H_
