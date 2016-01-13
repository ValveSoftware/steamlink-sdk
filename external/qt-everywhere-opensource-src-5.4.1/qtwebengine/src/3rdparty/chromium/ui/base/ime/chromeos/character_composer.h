// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_CHARACTER_COMPOSER_H_
#define UI_BASE_IME_CHROMEOS_CHARACTER_COMPOSER_H_

#include <vector>

#include "base/strings/string_util.h"
#include "ui/base/ui_base_export.h"

namespace ui {
class KeyEvent;

// A class to recognize compose and dead key sequence.
// Outputs composed character.
class UI_BASE_EXPORT CharacterComposer {
 public:
  CharacterComposer();
  ~CharacterComposer();

  void Reset();

  // Filters keypress.
  // Returns true if the keypress is recognized as a part of composition
  // sequence.
  // Fabricated events which don't have the native event, are not supported.
  bool FilterKeyPress(const ui::KeyEvent& event);

  // Returns a string consisting of composed character.
  // Empty string is returned when there is no composition result.
  const base::string16& composed_character() const {
    return composed_character_;
  }

  // Returns the preedit string.
  const base::string16& preedit_string() const { return preedit_string_; }

 private:
  friend class CharacterComposerTest;

  // An enum to describe composition mode.
  enum CompositionMode {
    // This is the initial state.
    // Composite a character with dead-keys and compose-key.
    KEY_SEQUENCE_MODE,
    // Composite a character with a hexadecimal unicode sequence.
    HEX_MODE,
  };

  // Filters keypress using IBus defined value.
  // Returns true if the keypress is recognized as a part of composition
  // sequence.
  // |keyval| must be a GDK_KEY_* constant.
  // |keycode| must be a X key code.
  // |flags| must be a combination of ui::EF_* flags.
  //
  // composed_character() returns non empty string when there is a character
  // composed after this method returns true.
  // preedit_string() returns non empty string when there is a preedit string
  // after this method returns true.
  // Return values of preedit_string() is empty after this method returns false.
  // composed_character() may have some characters which are consumed in this
  // composing session.
  //
  //
  // TODO(nona): Actually a X KeySym is passed to |keyval|, so we should use
  // XK_* rather than GDK_KEY_*.
  bool FilterKeyPressInternal(unsigned int keyval, unsigned int keycode,
                              int flags);

  // Filters keypress in key sequence mode.
  bool FilterKeyPressSequenceMode(unsigned int keyval, int flags);

  // Filters keypress in hexadecimal mode.
  bool FilterKeyPressHexMode(unsigned int keyval, unsigned int keycode,
                             int flags);

  // Commit a character composed from hexadecimal uncode sequence
  void CommitHex();

  // Updates preedit string in hexadecimal mode.
  void UpdatePreeditStringHexMode();

  // Remembers keypresses previously filtered.
  std::vector<unsigned int> compose_buffer_;

  // A string representing the composed character.
  base::string16 composed_character_;

  // Preedit string.
  base::string16 preedit_string_;

  // Composition mode which this instance is in.
  CompositionMode composition_mode_;

  DISALLOW_COPY_AND_ASSIGN(CharacterComposer);
};

}  // namespace ui

#endif  // UI_BASE_IME_CHROMEOS_CHARACTER_COMPOSER_H_
