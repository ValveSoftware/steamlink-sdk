// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/character_composer.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/gtk+/gdk/gdkkeysyms.h"
#include "ui/base/glib/glib_integers.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

using base::ASCIIToUTF16;

namespace ui {

class CharacterComposerTest : public testing::Test {
 protected:
  bool FilterKeyPress(CharacterComposer* character_composer,
                      uint key,
                      uint keycode,
                      int flags) {
    return character_composer->FilterKeyPressInternal(key, keycode, flags);
  }

  // Expects key is not filtered and no character is composed.
  void ExpectKeyNotFilteredWithKeyCode(CharacterComposer* character_composer,
                                       uint key,
                                       uint keycode,
                                       int flags) {
    EXPECT_FALSE(character_composer->FilterKeyPressInternal(key, keycode,
                                                            flags));
    EXPECT_TRUE(character_composer->composed_character().empty());
  }

  // Expects key is filtered and no character is composed.
  void ExpectKeyFilteredWithKeycode(CharacterComposer* character_composer,
                                    uint key,
                                    uint keycode,
                                    int flags) {
    EXPECT_TRUE(character_composer->FilterKeyPressInternal(key, keycode,
                                                           flags));
    EXPECT_TRUE(character_composer->composed_character().empty());
  }

  // Expects key is not filtered and no character is composed.
  void ExpectKeyNotFiltered(CharacterComposer* character_composer,
                            uint key,
                            int flags) {
    ExpectKeyNotFilteredWithKeyCode(character_composer, key, 0, flags);
  }

  // Expects key is filtered and no character is composed.
  void ExpectKeyFiltered(CharacterComposer* character_composer,
                         uint key,
                         int flags) {
    ExpectKeyFilteredWithKeycode(character_composer, key, 0, flags);
  }

  // Expects |expected_character| is composed after sequence [key1, key2].
  void ExpectCharacterComposed(CharacterComposer* character_composer,
                               uint key1,
                             uint key2,
                               int flags,
                               const base::string16& expected_character) {
    ExpectKeyFiltered(character_composer, key1, flags);
    EXPECT_TRUE(character_composer->FilterKeyPressInternal(key2, 0, flags));
    EXPECT_EQ(expected_character, character_composer->composed_character());
  }

  // Expects |expected_character| is composed after sequence [key1, key2, key3].
  void ExpectCharacterComposed(CharacterComposer* character_composer,
                               uint key1,
                               uint key2,
                               uint key3,
                               int flags,
                               const base::string16& expected_character) {
    ExpectKeyFiltered(character_composer, key1, flags);
    ExpectCharacterComposed(character_composer, key2, key3, flags,
                            expected_character);
  }

  // Expects |expected_character| is composed after sequence [key1, key2, key3,
  // key 4].
  void ExpectCharacterComposed(CharacterComposer* character_composer,
                               uint key1,
                               uint key2,
                               uint key3,
                               uint key4,
                               int flags,
                               const base::string16& expected_character) {
    ExpectKeyFiltered(character_composer, key1, flags);
    ExpectCharacterComposed(character_composer, key2, key3, key4, flags,
                            expected_character);
  }

  // Expects |expected_character| is composed after sequence [key1, key2, key3,
  // key 4, key5].
  void ExpectCharacterComposed(CharacterComposer* character_composer,
                               uint key1,
                               uint key2,
                               uint key3,
                               uint key4,
                               uint key5,
                               int flags,
                               const base::string16& expected_character) {
    ExpectKeyFiltered(character_composer, key1, flags);
    ExpectCharacterComposed(character_composer, key2, key3, key4, key5, flags,
                            expected_character);
  }

  // Expects |expected_character| is composed after sequence [key1, key2, key3,
  // key 4, key5, key6].
  void ExpectCharacterComposed(CharacterComposer* character_composer,
                               uint key1,
                               uint key2,
                               uint key3,
                               uint key4,
                               uint key5,
                               uint key6,
                               int flags,
                               const base::string16& expected_character) {
    ExpectKeyFiltered(character_composer, key1, flags);
    ExpectCharacterComposed(character_composer, key2, key3, key4, key5, key6,
                            flags, expected_character);
  }

  // Expects |expected_character| is composed after sequence [{key1, keycode1}].
  void ExpectCharacterComposedWithKeyCode(
      CharacterComposer* character_composer,
      uint key1, uint keycode1,
      int flags,
      const base::string16& expected_character) {
    EXPECT_TRUE(character_composer->FilterKeyPressInternal(key1, keycode1,
                                                           flags));
    EXPECT_EQ(expected_character, character_composer->composed_character());
  }
};

TEST_F(CharacterComposerTest, InitialState) {
  CharacterComposer character_composer;
  EXPECT_TRUE(character_composer.composed_character().empty());
}

TEST_F(CharacterComposerTest, NormalKeyIsNotFiltered) {
  CharacterComposer character_composer;
  ExpectKeyNotFiltered(&character_composer, GDK_KEY_B, 0);
  ExpectKeyNotFiltered(&character_composer, GDK_KEY_Z, 0);
  ExpectKeyNotFiltered(&character_composer, GDK_KEY_c, 0);
  ExpectKeyNotFiltered(&character_composer, GDK_KEY_m, 0);
  ExpectKeyNotFiltered(&character_composer, GDK_KEY_0, 0);
  ExpectKeyNotFiltered(&character_composer, GDK_KEY_1, 0);
  ExpectKeyNotFiltered(&character_composer, GDK_KEY_8, 0);
}

TEST_F(CharacterComposerTest, PartiallyMatchingSequence) {
  CharacterComposer character_composer;

  // Composition with sequence ['dead acute', '1'] will fail.
  ExpectKeyFiltered(&character_composer, GDK_KEY_dead_acute, 0);
  ExpectKeyFiltered(&character_composer, GDK_KEY_1, 0);

  // Composition with sequence ['dead acute', 'dead circumflex', '1'] will fail.
  ExpectKeyFiltered(&character_composer, GDK_KEY_dead_acute, 0);
  ExpectKeyFiltered(&character_composer, GDK_KEY_dead_circumflex, 0);
  ExpectKeyFiltered(&character_composer, GDK_KEY_1, 0);
}

TEST_F(CharacterComposerTest, FullyMatchingSequences) {
  CharacterComposer character_composer;
  // LATIN SMALL LETTER A WITH ACUTE
  ExpectCharacterComposed(&character_composer, GDK_KEY_dead_acute, GDK_KEY_a, 0,
                          base::string16(1, 0x00E1));
  // LATIN CAPITAL LETTER A WITH ACUTE
  ExpectCharacterComposed(&character_composer, GDK_KEY_dead_acute, GDK_KEY_A, 0,
                          base::string16(1, 0x00C1));
  // GRAVE ACCENT
  ExpectCharacterComposed(&character_composer, GDK_KEY_dead_grave,
                          GDK_KEY_dead_grave, 0, base::string16(1, 0x0060));
  // LATIN SMALL LETTER A WITH CIRCUMFLEX AND ACUTE
  ExpectCharacterComposed(&character_composer, GDK_KEY_dead_acute,
                          GDK_KEY_dead_circumflex, GDK_KEY_a, 0,
                          base::string16(1, 0x1EA5));
  // LATIN CAPITAL LETTER U WITH HORN AND GRAVE
  ExpectCharacterComposed(&character_composer, GDK_KEY_dead_grave,
                          GDK_KEY_dead_horn, GDK_KEY_U, 0,
                          base::string16(1, 0x1EEA));
  // LATIN CAPITAL LETTER C WITH CEDILLA
  ExpectCharacterComposed(&character_composer, GDK_KEY_dead_acute, GDK_KEY_C, 0,
                          base::string16(1, 0x00C7));
  // LATIN SMALL LETTER C WITH CEDILLA
  ExpectCharacterComposed(&character_composer, GDK_KEY_dead_acute, GDK_KEY_c, 0,
                          base::string16(1, 0x00E7));
}

TEST_F(CharacterComposerTest, FullyMatchingSequencesAfterMatchingFailure) {
  CharacterComposer character_composer;
  // Composition with sequence ['dead acute', 'dead circumflex', '1'] will fail.
  ExpectKeyFiltered(&character_composer, GDK_KEY_dead_acute, 0);
  ExpectKeyFiltered(&character_composer, GDK_KEY_dead_circumflex, 0);
  ExpectKeyFiltered(&character_composer, GDK_KEY_1, 0);
  // LATIN SMALL LETTER A WITH CIRCUMFLEX AND ACUTE
  ExpectCharacterComposed(&character_composer, GDK_KEY_dead_acute,
                          GDK_KEY_dead_circumflex, GDK_KEY_a, 0,
                          base::string16(1, 0x1EA5));
}

TEST_F(CharacterComposerTest, ComposedCharacterIsClearedAfterReset) {
  CharacterComposer character_composer;
  ExpectCharacterComposed(&character_composer, GDK_KEY_dead_acute, GDK_KEY_a, 0,
                          base::string16(1, 0x00E1));
  character_composer.Reset();
  EXPECT_TRUE(character_composer.composed_character().empty());
}

TEST_F(CharacterComposerTest, CompositionStateIsClearedAfterReset) {
  CharacterComposer character_composer;
  // Even though sequence ['dead acute', 'a'] will compose 'a with acute',
  // no character is composed here because of reset.
  ExpectKeyFiltered(&character_composer, GDK_KEY_dead_acute, 0);
  character_composer.Reset();
  ExpectKeyNotFiltered(&character_composer, GDK_KEY_a, 0);
}

TEST_F(CharacterComposerTest, KeySequenceCompositionPreedit) {
  CharacterComposer character_composer;
  // LATIN SMALL LETTER A WITH ACUTE
  // preedit_string() is always empty in key sequence composition mode.
  ExpectKeyFiltered(&character_composer, GDK_KEY_dead_acute, 0);
  EXPECT_TRUE(character_composer.preedit_string().empty());
  EXPECT_TRUE(FilterKeyPress(&character_composer, GDK_KEY_a, 0, 0));
  EXPECT_EQ(base::string16(1, 0x00E1), character_composer.composed_character());
  EXPECT_TRUE(character_composer.preedit_string().empty());
}

// ComposeCheckerWithCompactTable in character_composer.cc is depending on the
// assumption that the data in gtkimcontextsimpleseqs.h is correctly ordered.
TEST_F(CharacterComposerTest, MainTableIsCorrectlyOrdered) {
  // This file is included here intentionally, instead of the top of the file,
  // because including this file at the top of the file will define a
  // global constant and contaminate the global namespace.
#include "third_party/gtk+/gtk/gtkimcontextsimpleseqs.h"
  const int index_size = 26;
  const int index_stride = 6;

  // Verify that the index is correctly ordered
  for (int i = 1; i < index_size; ++i) {
    const int index_key_prev = gtk_compose_seqs_compact[(i - 1)*index_stride];
    const int index_key = gtk_compose_seqs_compact[i*index_stride];
    EXPECT_TRUE(index_key > index_key_prev);
  }

  // Verify that the sequenes are correctly ordered
  struct {
    int operator()(const uint16* l, const uint16* r, int length) const{
      for (int i = 0; i < length; ++i) {
        if (l[i] > r[i])
          return 1;
        if (l[i] < r[i])
          return -1;
      }
      return 0;
    }
  } compare_sequence;

  for (int i = 0; i < index_size; ++i) {
    for (int length = 1; length < index_stride - 1; ++length) {
      const int index_begin = gtk_compose_seqs_compact[i*index_stride + length];
      const int index_end =
          gtk_compose_seqs_compact[i*index_stride + length + 1];
      const int stride = length + 1;
      for (int index = index_begin + stride; index < index_end;
           index += stride) {
        const uint16* sequence = &gtk_compose_seqs_compact[index];
        const uint16* sequence_prev = sequence - stride;
        EXPECT_EQ(1, compare_sequence(sequence, sequence_prev, length));
      }
    }
  }
}

TEST_F(CharacterComposerTest, HexadecimalComposition) {
  CharacterComposer character_composer;
  // HIRAGANA LETTER A (U+3042)
  ExpectKeyFiltered(&character_composer, GDK_KEY_U,
                    EF_SHIFT_DOWN | EF_CONTROL_DOWN);
  ExpectCharacterComposed(&character_composer, GDK_KEY_3, GDK_KEY_0, GDK_KEY_4,
                          GDK_KEY_2, GDK_KEY_space, 0,
                          base::string16(1, 0x3042));
  // MUSICAL KEYBOARD (U+1F3B9)
  const base::char16 kMusicalKeyboard[] = {0xd83c, 0xdfb9};
  ExpectKeyFiltered(&character_composer, GDK_KEY_U,
                    EF_SHIFT_DOWN | EF_CONTROL_DOWN);
  ExpectCharacterComposed(&character_composer, GDK_KEY_1, GDK_KEY_f, GDK_KEY_3,
                          GDK_KEY_b, GDK_KEY_9, GDK_KEY_Return, 0,
                          base::string16(kMusicalKeyboard,
                                   kMusicalKeyboard +
                                   arraysize(kMusicalKeyboard)));
}

TEST_F(CharacterComposerTest, HexadecimalCompositionPreedit) {
  CharacterComposer character_composer;
  // HIRAGANA LETTER A (U+3042)
  ExpectKeyFiltered(&character_composer, GDK_KEY_U,
                    EF_SHIFT_DOWN | EF_CONTROL_DOWN);
  EXPECT_EQ(ASCIIToUTF16("u"), character_composer.preedit_string());
  ExpectKeyFiltered(&character_composer, GDK_KEY_3, 0);
  EXPECT_EQ(ASCIIToUTF16("u3"), character_composer.preedit_string());
  ExpectKeyFiltered(&character_composer, GDK_KEY_0, 0);
  EXPECT_EQ(ASCIIToUTF16("u30"), character_composer.preedit_string());
  ExpectKeyFiltered(&character_composer, GDK_KEY_4, 0);
  EXPECT_EQ(ASCIIToUTF16("u304"), character_composer.preedit_string());
  ExpectKeyFiltered(&character_composer, GDK_KEY_a, 0);
  EXPECT_EQ(ASCIIToUTF16("u304a"), character_composer.preedit_string());
  ExpectKeyFiltered(&character_composer, GDK_KEY_BackSpace, 0);
  EXPECT_EQ(ASCIIToUTF16("u304"), character_composer.preedit_string());
  ExpectCharacterComposed(&character_composer, GDK_KEY_2, GDK_KEY_Return, 0,
                          base::string16(1, 0x3042));
  EXPECT_EQ(ASCIIToUTF16(""), character_composer.preedit_string());

  // Sequence with an ignored character ('x') and Escape.
  ExpectKeyFiltered(&character_composer, GDK_KEY_U,
                    EF_SHIFT_DOWN | EF_CONTROL_DOWN);
  EXPECT_EQ(ASCIIToUTF16("u"), character_composer.preedit_string());
  ExpectKeyFiltered(&character_composer, GDK_KEY_3, 0);
  EXPECT_EQ(ASCIIToUTF16("u3"), character_composer.preedit_string());
  ExpectKeyFiltered(&character_composer, GDK_KEY_0, 0);
  EXPECT_EQ(ASCIIToUTF16("u30"), character_composer.preedit_string());
  ExpectKeyFiltered(&character_composer, GDK_KEY_x, 0);
  EXPECT_EQ(ASCIIToUTF16("u30"), character_composer.preedit_string());
  ExpectKeyFiltered(&character_composer, GDK_KEY_4, 0);
  EXPECT_EQ(ASCIIToUTF16("u304"), character_composer.preedit_string());
  ExpectKeyFiltered(&character_composer, GDK_KEY_2, 0);
  EXPECT_EQ(ASCIIToUTF16("u3042"), character_composer.preedit_string());
  ExpectKeyFiltered(&character_composer, GDK_KEY_Escape, 0);
  EXPECT_EQ(ASCIIToUTF16(""), character_composer.preedit_string());
}

TEST_F(CharacterComposerTest, HexadecimalCompositionWithNonHexKey) {
  CharacterComposer character_composer;

  // Sequence [Ctrl+Shift+U, x, space] does not compose a character.
  ExpectKeyFiltered(&character_composer, GDK_KEY_U,
                    EF_SHIFT_DOWN | EF_CONTROL_DOWN);
  ExpectKeyFiltered(&character_composer, GDK_KEY_x, 0);
  ExpectKeyFiltered(&character_composer, GDK_KEY_space, 0);
  EXPECT_TRUE(character_composer.composed_character().empty());

  // HIRAGANA LETTER A (U+3042) with a sequence [3, 0, x, 4, 2].
  ExpectKeyFiltered(&character_composer, GDK_KEY_U,
                    EF_SHIFT_DOWN | EF_CONTROL_DOWN);
  ExpectCharacterComposed(&character_composer, GDK_KEY_3, GDK_KEY_0, GDK_KEY_x,
                          GDK_KEY_4, GDK_KEY_2, GDK_KEY_space, 0,
                          base::string16(1, 0x3042));
}

TEST_F(CharacterComposerTest, HexadecimalCompositionWithAdditionalModifiers) {
  CharacterComposer character_composer;

  // Ctrl+Shift+Alt+U
  // HIRAGANA LETTER A (U+3042)
  ExpectKeyFiltered(&character_composer, GDK_KEY_U,
                    EF_SHIFT_DOWN | EF_CONTROL_DOWN | EF_ALT_DOWN);
  ExpectCharacterComposed(&character_composer, GDK_KEY_3, GDK_KEY_0, GDK_KEY_4,
                          GDK_KEY_2, GDK_KEY_space, 0,
                          base::string16(1, 0x3042));

  // Ctrl+Shift+u (CapsLock enabled)
  ExpectKeyNotFiltered(&character_composer, GDK_KEY_u,
                       EF_SHIFT_DOWN | EF_CONTROL_DOWN | EF_CAPS_LOCK_DOWN);
}

TEST_F(CharacterComposerTest, CancelHexadecimalComposition) {
  CharacterComposer character_composer;
  // Cancel composition with ESC.
  ExpectKeyFiltered(&character_composer, GDK_KEY_U,
                    EF_SHIFT_DOWN | EF_CONTROL_DOWN);
  ExpectKeyFiltered(&character_composer, GDK_KEY_1, 0);
  ExpectKeyFiltered(&character_composer, GDK_KEY_Escape, 0);

  // Now we can start composition again since the last composition was
  // cancelled.
  // HIRAGANA LETTER A (U+3042)
  ExpectKeyFiltered(&character_composer, GDK_KEY_U,
                    EF_SHIFT_DOWN | EF_CONTROL_DOWN);
  ExpectCharacterComposed(&character_composer, GDK_KEY_3, GDK_KEY_0, GDK_KEY_4,
                          GDK_KEY_2, GDK_KEY_space, 0,
                          base::string16(1, 0x3042));
}

TEST_F(CharacterComposerTest, HexadecimalCompositionWithBackspace) {
  CharacterComposer character_composer;
  // HIRAGANA LETTER A (U+3042)
  ExpectKeyFiltered(&character_composer, GDK_KEY_U,
                    EF_SHIFT_DOWN | EF_CONTROL_DOWN);
  ExpectKeyFiltered(&character_composer, GDK_KEY_3, 0);
  ExpectKeyFiltered(&character_composer, GDK_KEY_0, 0);
  ExpectKeyFiltered(&character_composer, GDK_KEY_f, 0);
  ExpectKeyFiltered(&character_composer, GDK_KEY_BackSpace, 0);
  ExpectCharacterComposed(&character_composer, GDK_KEY_4, GDK_KEY_2,
                          GDK_KEY_space, 0, base::string16(1, 0x3042));
}

TEST_F(CharacterComposerTest, CancelHexadecimalCompositionWithBackspace) {
  CharacterComposer character_composer;

  // Backspace just after Ctrl+Shift+U.
  ExpectKeyFiltered(&character_composer, GDK_KEY_U,
                    EF_SHIFT_DOWN | EF_CONTROL_DOWN);
  ExpectKeyFiltered(&character_composer, GDK_KEY_BackSpace, 0);
  ExpectKeyNotFiltered(&character_composer, GDK_KEY_3, 0);

  // Backspace twice after Ctrl+Shift+U and 3.
  ExpectKeyFiltered(&character_composer, GDK_KEY_U,
                    EF_SHIFT_DOWN | EF_CONTROL_DOWN);
  ExpectKeyFiltered(&character_composer, GDK_KEY_3, 0);
  ExpectKeyFiltered(&character_composer, GDK_KEY_BackSpace, 0);
  ExpectKeyFiltered(&character_composer, GDK_KEY_BackSpace, 0);
  ExpectKeyNotFiltered(&character_composer, GDK_KEY_3, 0);
}

TEST_F(CharacterComposerTest, HexadecimalCompositionPreeditWithModifierPressed)
{
  // This test case supposes X Window System uses 101 keyboard layout.
  CharacterComposer character_composer;
  const int control_shift =  EF_CONTROL_DOWN | EF_SHIFT_DOWN;
  // HIRAGANA LETTER A (U+3042)
  ExpectKeyFilteredWithKeycode(&character_composer,
                               GDK_KEY_U, ui::VKEY_U, control_shift);
  EXPECT_EQ(ASCIIToUTF16("u"), character_composer.preedit_string());
  ExpectKeyFilteredWithKeycode(&character_composer,
                               GDK_KEY_numbersign, ui::VKEY_3, control_shift);
  EXPECT_EQ(ASCIIToUTF16("u3"), character_composer.preedit_string());
  ExpectKeyFilteredWithKeycode(&character_composer,
                               GDK_KEY_parenright, ui::VKEY_0, control_shift);
  EXPECT_EQ(ASCIIToUTF16("u30"), character_composer.preedit_string());
  ExpectKeyFilteredWithKeycode(&character_composer,
                               GDK_KEY_dollar, ui::VKEY_4, control_shift);
  EXPECT_EQ(ASCIIToUTF16("u304"), character_composer.preedit_string());
  ExpectKeyFilteredWithKeycode(&character_composer,
                               GDK_KEY_A, ui::VKEY_A, control_shift);
  EXPECT_EQ(ASCIIToUTF16("u304a"), character_composer.preedit_string());
  ExpectKeyFilteredWithKeycode(&character_composer,
                               GDK_KEY_BackSpace, ui::VKEY_BACK, control_shift);
  EXPECT_EQ(ASCIIToUTF16("u304"), character_composer.preedit_string());
  ExpectKeyFilteredWithKeycode(&character_composer,
                               GDK_KEY_at, ui::VKEY_2, control_shift);
  EXPECT_EQ(ASCIIToUTF16("u3042"), character_composer.preedit_string());
  ExpectCharacterComposedWithKeyCode(&character_composer,
                                     GDK_KEY_Return, ui::VKEY_RETURN,
                                     control_shift,
                                     base::string16(1, 0x3042));
  EXPECT_EQ(ASCIIToUTF16(""), character_composer.preedit_string());

  // Sequence with an ignored character (control + shift + 'x') and Escape.
  ExpectKeyFilteredWithKeycode(&character_composer,
                               GDK_KEY_U, ui::VKEY_U, control_shift);
  EXPECT_EQ(ASCIIToUTF16("u"), character_composer.preedit_string());
  ExpectKeyFilteredWithKeycode(&character_composer,
                               GDK_KEY_numbersign, ui::VKEY_3, control_shift);
  EXPECT_EQ(ASCIIToUTF16("u3"), character_composer.preedit_string());
  ExpectKeyFilteredWithKeycode(&character_composer,
                               GDK_KEY_parenright, ui::VKEY_0, control_shift);
  EXPECT_EQ(ASCIIToUTF16("u30"), character_composer.preedit_string());
  ExpectKeyFilteredWithKeycode(&character_composer,
                               GDK_KEY_X, ui::VKEY_X, control_shift);
  EXPECT_EQ(ASCIIToUTF16("u30"), character_composer.preedit_string());
  ExpectKeyFilteredWithKeycode(&character_composer,
                               GDK_KEY_dollar, ui::VKEY_4, control_shift);
  EXPECT_EQ(ASCIIToUTF16("u304"), character_composer.preedit_string());
  ExpectKeyFilteredWithKeycode(&character_composer,
                               GDK_KEY_at, ui::VKEY_2, control_shift);
  EXPECT_EQ(ASCIIToUTF16("u3042"), character_composer.preedit_string());
  ExpectKeyFilteredWithKeycode(&character_composer,
                               GDK_KEY_Escape, ui::VKEY_ESCAPE, control_shift);
  EXPECT_EQ(ASCIIToUTF16(""), character_composer.preedit_string());
}

TEST_F(CharacterComposerTest, InvalidHexadecimalSequence) {
  CharacterComposer character_composer;
  // U+FFFFFFFF
  ExpectKeyFiltered(&character_composer, GDK_KEY_U,
                    EF_SHIFT_DOWN | EF_CONTROL_DOWN);
  for (int i = 0; i < 8; ++i)
    ExpectKeyFiltered(&character_composer, GDK_KEY_f, 0);
  ExpectKeyFiltered(&character_composer, GDK_KEY_space, 0);

  // U+0000 (Actually, this is a valid unicode character, but we don't
  // compose a string with a character '\0')
  ExpectKeyFiltered(&character_composer, GDK_KEY_U,
                    EF_SHIFT_DOWN | EF_CONTROL_DOWN);
  for (int i = 0; i < 4; ++i)
    ExpectKeyFiltered(&character_composer, GDK_KEY_0, 0);
  ExpectKeyFiltered(&character_composer, GDK_KEY_space, 0);

  // U+10FFFF
  ExpectKeyFiltered(&character_composer, GDK_KEY_U,
                    EF_SHIFT_DOWN | EF_CONTROL_DOWN);
  ExpectKeyFiltered(&character_composer, GDK_KEY_1, 0);
  ExpectKeyFiltered(&character_composer, GDK_KEY_0, 0);
  for (int i = 0; i < 4; ++i)
    ExpectKeyFiltered(&character_composer, GDK_KEY_f, 0);
  ExpectKeyFiltered(&character_composer, GDK_KEY_space, 0);

  // U+110000
  ExpectKeyFiltered(&character_composer, GDK_KEY_U,
                    EF_SHIFT_DOWN | EF_CONTROL_DOWN);
  ExpectKeyFiltered(&character_composer, GDK_KEY_1, 0);
  ExpectKeyFiltered(&character_composer, GDK_KEY_1, 0);
  for (int i = 0; i < 4; ++i)
    ExpectKeyFiltered(&character_composer, GDK_KEY_0, 0);
  ExpectKeyFiltered(&character_composer, GDK_KEY_space, 0);
}

TEST_F(CharacterComposerTest, HexadecimalSequenceAndDeadKey) {
  CharacterComposer character_composer;
  // LATIN SMALL LETTER A WITH ACUTE
  ExpectCharacterComposed(&character_composer, GDK_KEY_dead_acute, GDK_KEY_a, 0,
                          base::string16(1, 0x00E1));
  // HIRAGANA LETTER A (U+3042) with dead_acute ignored.
  ExpectKeyFiltered(&character_composer, GDK_KEY_U,
                    EF_SHIFT_DOWN | EF_CONTROL_DOWN);
  ExpectCharacterComposed(&character_composer, GDK_KEY_3, GDK_KEY_0,
                          GDK_KEY_dead_acute, GDK_KEY_4,  GDK_KEY_2,
                          GDK_KEY_space, 0, base::string16(1, 0x3042));
  // LATIN CAPITAL LETTER U WITH ACUTE while 'U' is pressed with Ctrl+Shift.
  ExpectKeyFiltered(&character_composer, GDK_KEY_dead_acute, 0);
  EXPECT_TRUE(FilterKeyPress(&character_composer, GDK_KEY_U, 0,
                             EF_SHIFT_DOWN | EF_CONTROL_DOWN));
  EXPECT_EQ(base::string16(1, 0x00DA), character_composer.composed_character());
}

TEST_F(CharacterComposerTest, BlacklistedKeyeventsTest) {
  CharacterComposer character_composer;
  EXPECT_TRUE(FilterKeyPress(&character_composer, GDK_KEY_dead_acute, 0, 0));
  EXPECT_FALSE(FilterKeyPress(&character_composer, GDK_KEY_s, 0, 0));
  ASSERT_EQ(1U, character_composer.composed_character().size());
  EXPECT_EQ(GDK_KEY_apostrophe, character_composer.composed_character().at(0));
}

}  // namespace ui
