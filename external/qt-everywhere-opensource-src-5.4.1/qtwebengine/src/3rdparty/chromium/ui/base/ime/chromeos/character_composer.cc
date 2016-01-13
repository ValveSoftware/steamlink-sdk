// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/character_composer.h"

#include <algorithm>
#include <iterator>

#include "base/strings/utf_string_conversions.h"
#include "base/third_party/icu/icu_utf.h"
// Note for Gtk removal: gdkkeysyms.h only contains a set of
// '#define GDK_KeyName 0xNNNN' macros and does not #include any Gtk headers.
#include "third_party/gtk+/gdk/gdkkeysyms.h"

#include "ui/base/glib/glib_integers.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"

// Note for Gtk removal: gtkimcontextsimpleseqs.h does not #include any Gtk
// headers and only contains one big guint16 array |gtk_compose_seqs_compact|
// which defines the main compose table. The table has internal linkage.
// The order of header inclusion is out of order because
// gtkimcontextsimpleseqs.h depends on guint16, which is defined in
// "ui/base/glib/glib_integers.h".
#include "third_party/gtk+/gtk/gtkimcontextsimpleseqs.h"

namespace {

// A black list for not composing dead keys. Once the key combination is listed
// below, the dead key won't work even when this is listed in
// gtkimcontextsimpleseqs.h. This only supports two keyevent sequenses.
// TODO(nona): Remove this hack.
const struct BlackListedDeadKey {
  uint32 first_key;  // target first key event.
  uint32 second_key;  // target second key event.
  uint32 output_char;  // the character to be inserted if the filter is matched.
  bool consume;  // true if the original key event will be consumed.
} kBlackListedDeadKeys[] = {
  { GDK_KEY_dead_acute, GDK_KEY_m, GDK_KEY_apostrophe, false },
  { GDK_KEY_dead_acute, GDK_KEY_s, GDK_KEY_apostrophe, false },
  { GDK_KEY_dead_acute, GDK_KEY_t, GDK_KEY_apostrophe, false },
  { GDK_KEY_dead_acute, GDK_KEY_v, GDK_KEY_apostrophe, false },
  { GDK_KEY_dead_acute, GDK_KEY_dead_acute, GDK_KEY_apostrophe, true },
};

typedef std::vector<unsigned int> ComposeBufferType;

// An iterator class to apply std::lower_bound for composition table.
class SequenceIterator
    : public std::iterator<std::random_access_iterator_tag, const uint16*> {
 public:
  SequenceIterator() : ptr_(NULL), stride_(0) {}
  SequenceIterator(const uint16* ptr, int stride)
      : ptr_(ptr), stride_(stride) {}

  const uint16* ptr() const {return ptr_;}
  int stride() const {return stride_;}

  SequenceIterator& operator++() {
    ptr_ += stride_;
    return *this;
  }
  SequenceIterator& operator+=(int n) {
    ptr_ += stride_*n;
    return *this;
  }

  const uint16* operator*() const {return ptr_;}

 private:
  const uint16* ptr_;
  int stride_;
};

inline SequenceIterator operator+(const SequenceIterator& l, int r) {
  return SequenceIterator(l) += r;
}

inline int operator-(const SequenceIterator& l, const SequenceIterator& r) {
  const int d = l.ptr() - r.ptr();
  DCHECK(l.stride() == r.stride() && l.stride() > 0 && d%l.stride() == 0);
  return d/l.stride();
}

inline bool operator==(const SequenceIterator& l, const SequenceIterator& r) {
  DCHECK(l.stride() == r.stride());
  return l.ptr() == r.ptr();
}

inline bool operator!=(const SequenceIterator& l, const SequenceIterator& r) {
  return !(l == r);
}

// A function to compare key value.
inline int CompareSequenceValue(unsigned int l, unsigned int r) {
  return (l > r) ? 1 : ((l < r) ? -1 : 0);
}

// A template to make |CompareFunc| work like operator<.
// |CompareFunc| is required to implement a member function,
// int operator()(const ComposeBufferType& l, const uint16* r) const.
template<typename CompareFunc>
struct ComparatorAdoptor {
  bool operator()(const ComposeBufferType& l, const uint16* r) const {
    return CompareFunc()(l, r) == -1;
  }
  bool operator()(const uint16* l, const ComposeBufferType& r) const {
    return CompareFunc()(r, l) == 1;
  }
};

class ComposeChecker {
 public:
  // This class does not take the ownership of |data|, |data| should be alive
  // for the lifetime of the object.
  // |data| is a pointer to the head of an array of
  // length (|max_sequence_length| + 2)*|n_sequences|.
  // Every (|max_sequence_length| + 2) elements of |data| represent an entry.
  // First |max_sequence_length| elements of an entry is the sequecne which
  // composes the character represented by the last two elements of the entry.
  ComposeChecker(const uint16* data, int max_sequence_length, int n_sequences);
  bool CheckSequence(const ComposeBufferType& sequence,
                     uint32* composed_character) const;

 private:
  struct CompareSequence {
    int operator()(const ComposeBufferType& l, const uint16* r) const;
  };

  // This class does not take the ownership of |data_|,
  // the dtor does not delete |data_|.
  const uint16* data_;
  int max_sequence_length_;
  int n_sequences_;
  int row_stride_;

  DISALLOW_COPY_AND_ASSIGN(ComposeChecker);
};

ComposeChecker::ComposeChecker(const uint16* data,
                               int max_sequence_length,
                               int n_sequences)
    : data_(data),
      max_sequence_length_(max_sequence_length),
      n_sequences_(n_sequences),
      row_stride_(max_sequence_length + 2) {
}

bool ComposeChecker::CheckSequence(const ComposeBufferType& sequence,
                                   uint32* composed_character) const {
  const int sequence_length = sequence.size();
  if (sequence_length > max_sequence_length_)
    return false;
  // Find sequence in the table.
  const SequenceIterator begin(data_, row_stride_);
  const SequenceIterator end = begin + n_sequences_;
  const SequenceIterator found = std::lower_bound(
      begin, end, sequence, ComparatorAdoptor<CompareSequence>());
  if (found == end || CompareSequence()(sequence, *found) != 0)
    return false;

  if (sequence_length == max_sequence_length_ ||
      (*found)[sequence_length] == 0) {
    // |found| is not partially matching. It's fully matching.
    if (found + 1 == end ||
        CompareSequence()(sequence, *(found + 1)) != 0) {
      // There is no composition longer than |found| which matches to
      // |sequence|.
      const uint32 value = ((*found)[max_sequence_length_] << 16) |
          (*found)[max_sequence_length_ + 1];
      *composed_character = value;
    }
  }
  return true;
}

int ComposeChecker::CompareSequence::operator()(const ComposeBufferType& l,
                                                const uint16* r) const {
  for(size_t i = 0; i < l.size(); ++i) {
    const int compare_result = CompareSequenceValue(l[i], r[i]);
    if(compare_result)
      return compare_result;
  }
  return 0;
}


class ComposeCheckerWithCompactTable {
 public:
  // This class does not take the ownership of |data|, |data| should be alive
  // for the lifetime of the object.
  // First |index_size|*|index_stride| elements of |data| are an index table.
  // Every |index_stride| elements of an index table are an index entry.
  // If you are checking with a sequence of length N beginning with character C,
  // you have to find an index entry whose first element is C, then get the N-th
  // element of the index entry as the index.
  // The index is pointing the element of |data| where the composition table for
  // sequences of length N beginning with C is placed.

  ComposeCheckerWithCompactTable(const uint16* data,
                                 int max_sequence_length,
                                 int index_size,
                                 int index_stride);
  bool CheckSequence(const ComposeBufferType& sequence,
                     uint32* composed_character) const;

 private:
  struct CompareSequenceFront {
    int operator()(const ComposeBufferType& l, const uint16* r) const;
  };
  struct CompareSequenceSkipFront {
    int operator()(const ComposeBufferType& l, const uint16* r) const;
  };

  // This class does not take the ownership of |data_|,
  // the dtor does not delete |data_|.
  const uint16* data_;
  int max_sequence_length_;
  int index_size_;
  int index_stride_;
};

ComposeCheckerWithCompactTable::ComposeCheckerWithCompactTable(
    const uint16* data,
    int max_sequence_length,
    int index_size,
    int index_stride)
    : data_(data),
      max_sequence_length_(max_sequence_length),
      index_size_(index_size),
      index_stride_(index_stride) {
}

bool ComposeCheckerWithCompactTable::CheckSequence(
    const ComposeBufferType& sequence,
    uint32* composed_character) const {
  const int compose_length = sequence.size();
  if (compose_length > max_sequence_length_)
    return false;
  // Find corresponding index for the first keypress.
  const SequenceIterator index_begin(data_, index_stride_);
  const SequenceIterator index_end = index_begin + index_size_;
  const SequenceIterator index =
      std::lower_bound(index_begin, index_end, sequence,
                       ComparatorAdoptor<CompareSequenceFront>());
  if (index == index_end || CompareSequenceFront()(sequence, *index) != 0)
    return false;
  if (compose_length == 1)
    return true;
  // Check for composition sequences.
  for (int length = compose_length - 1; length < max_sequence_length_;
       ++length) {
    const uint16* table = data_ + (*index)[length];
    const uint16* table_next = data_ + (*index)[length + 1];
    if (table_next > table) {
      // There are composition sequences for this |length|.
      const int row_stride = length + 1;
      const int n_sequences = (table_next - table)/row_stride;
      const SequenceIterator table_begin(table, row_stride);
      const SequenceIterator table_end = table_begin + n_sequences;
      const SequenceIterator found =
          std::lower_bound(table_begin, table_end, sequence,
                           ComparatorAdoptor<CompareSequenceSkipFront>());
      if (found != table_end &&
          CompareSequenceSkipFront()(sequence, *found) == 0) {
        if (length == compose_length - 1)  // Exact match.
          *composed_character = (*found)[length];
        return true;
      }
    }
  }
  return false;
}

int ComposeCheckerWithCompactTable::CompareSequenceFront::operator()(
    const ComposeBufferType& l, const uint16* r) const {
  return CompareSequenceValue(l[0], r[0]);
}

int ComposeCheckerWithCompactTable::CompareSequenceSkipFront::operator()(
    const ComposeBufferType& l, const uint16* r) const {
  for(size_t i = 1; i < l.size(); ++i) {
    const int compare_result = CompareSequenceValue(l[i], r[i - 1]);
    if(compare_result)
      return compare_result;
  }
  return 0;
}


// Additional table.

// The difference between this and the default input method is the handling
// of C+acute - this method produces C WITH CEDILLA rather than C WITH ACUTE.
// For languages that use CCedilla and not acute, this is the preferred mapping,
// and is particularly important for pt_BR, where the us-intl keyboard is
// used extensively.

const uint16 cedilla_compose_seqs[] = {
  // LATIN_CAPITAL_LETTER_C_WITH_CEDILLA
  GDK_KEY_dead_acute, GDK_KEY_C, 0, 0, 0, 0x00C7,
  // LATIN_SMALL_LETTER_C_WITH_CEDILLA
  GDK_KEY_dead_acute, GDK_KEY_c, 0, 0, 0, 0x00E7,
  // LATIN_CAPITAL_LETTER_C_WITH_CEDILLA
  GDK_KEY_Multi_key, GDK_KEY_apostrophe, GDK_KEY_C, 0, 0, 0x00C7,
  // LATIN_SMALL_LETTER_C_WITH_CEDILLA
  GDK_KEY_Multi_key, GDK_KEY_apostrophe, GDK_KEY_c, 0, 0, 0x00E7,
  // LATIN_CAPITAL_LETTER_C_WITH_CEDILLA
  GDK_KEY_Multi_key, GDK_KEY_C, GDK_KEY_apostrophe, 0, 0, 0x00C7,
  // LATIN_SMALL_LETTER_C_WITH_CEDILLA
  GDK_KEY_Multi_key, GDK_KEY_c, GDK_KEY_apostrophe, 0, 0, 0x00E7,
};

bool KeypressShouldBeIgnored(unsigned int keyval) {
  switch(keyval) {
    case GDK_KEY_Shift_L:
    case GDK_KEY_Shift_R:
    case GDK_KEY_Control_L:
    case GDK_KEY_Control_R:
    case GDK_KEY_Caps_Lock:
    case GDK_KEY_Shift_Lock:
    case GDK_KEY_Meta_L:
    case GDK_KEY_Meta_R:
    case GDK_KEY_Alt_L:
    case GDK_KEY_Alt_R:
    case GDK_KEY_Super_L:
    case GDK_KEY_Super_R:
    case GDK_KEY_Hyper_L:
    case GDK_KEY_Hyper_R:
    case GDK_KEY_Mode_switch:
    case GDK_KEY_ISO_Level3_Shift:
      return true;
    default:
      return false;
  }
}

bool CheckCharacterComposeTable(const ComposeBufferType& sequence,
                                uint32* composed_character) {
  // Check cedilla compose table.
  const ComposeChecker kCedillaComposeChecker(
      cedilla_compose_seqs, 4, arraysize(cedilla_compose_seqs)/(4 + 2));
  if (kCedillaComposeChecker.CheckSequence(sequence, composed_character))
    return true;

  // Check main compose table.
  const ComposeCheckerWithCompactTable kMainComposeChecker(
      gtk_compose_seqs_compact, 5, 24, 6);
  if (kMainComposeChecker.CheckSequence(sequence, composed_character))
    return true;

  return false;
}

// Converts |character| to UTF16 string.
// Returns false when |character| is not a valid character.
bool UTF32CharacterToUTF16(uint32 character, base::string16* output) {
  output->clear();
  // Reject invalid character. (e.g. codepoint greater than 0x10ffff)
  if (!CBU_IS_UNICODE_CHAR(character))
    return false;
  if (character) {
    output->resize(CBU16_LENGTH(character));
    size_t i = 0;
    CBU16_APPEND_UNSAFE(&(*output)[0], i, character);
  }
  return true;
}

// Returns an hexadecimal digit integer (0 to 15) corresponding to |keyval|.
// -1 is returned when |keyval| cannot be a hexadecimal digit.
int KeyvalToHexDigit(unsigned int keyval) {
  if (GDK_KEY_0 <= keyval && keyval <= GDK_KEY_9)
    return keyval - GDK_KEY_0;
  if (GDK_KEY_a <= keyval && keyval <= GDK_KEY_f)
    return keyval - GDK_KEY_a + 10;
  if (GDK_KEY_A <= keyval && keyval <= GDK_KEY_F)
    return keyval - GDK_KEY_A + 10;
  return -1;  // |keyval| cannot be a hexadecimal digit.
}

// Returns an hexadecimal digit integer (0 to 15) corresponding to |keycode|.
// -1 is returned when |keycode| cannot be a hexadecimal digit.
int KeycodeToHexDigit(unsigned int keycode) {
  if (ui::VKEY_0 <= keycode && keycode <= ui::VKEY_9)
    return keycode - ui::VKEY_0;
  if (ui::VKEY_A <= keycode && keycode <= ui::VKEY_F)
    return keycode - ui::VKEY_A + 10;
  return -1;  // |keycode| cannot be a hexadecimal digit.
}

}  // namespace

namespace ui {

CharacterComposer::CharacterComposer() : composition_mode_(KEY_SEQUENCE_MODE) {}

CharacterComposer::~CharacterComposer() {}

void CharacterComposer::Reset() {
  compose_buffer_.clear();
  composed_character_.clear();
  preedit_string_.clear();
  composition_mode_ = KEY_SEQUENCE_MODE;
}

bool CharacterComposer::FilterKeyPress(const ui::KeyEvent& event) {
  uint32 keyval = event.platform_keycode();
  if (!keyval ||
      (event.type() != ET_KEY_PRESSED && event.type() != ET_KEY_RELEASED))
    return false;

  return FilterKeyPressInternal(keyval, event.key_code(), event.flags());
}


bool CharacterComposer::FilterKeyPressInternal(unsigned int keyval,
                                               unsigned int keycode,
                                               int flags) {
  composed_character_.clear();
  preedit_string_.clear();

  // We don't care about modifier key presses.
  if(KeypressShouldBeIgnored(keyval))
    return false;

  // When the user presses Ctrl+Shift+U, maybe switch to HEX_MODE.
  // We don't care about other modifiers like Alt.  When CapsLock is down, we
  // do nothing because what we receive is Ctrl+Shift+u (not U).
  if (keyval == GDK_KEY_U && (flags & EF_SHIFT_DOWN) &&
      (flags & EF_CONTROL_DOWN)) {
    if (composition_mode_ == KEY_SEQUENCE_MODE && compose_buffer_.empty()) {
      // There is no ongoing composition.  Let's switch to HEX_MODE.
      composition_mode_ = HEX_MODE;
      UpdatePreeditStringHexMode();
      return true;
    }
  }

  // Filter key press in an appropriate manner.
  switch (composition_mode_) {
    case KEY_SEQUENCE_MODE:
      return FilterKeyPressSequenceMode(keyval, flags);
    case HEX_MODE:
      return FilterKeyPressHexMode(keyval, keycode, flags);
    default:
      NOTREACHED();
      return false;
  }
}

bool CharacterComposer::FilterKeyPressSequenceMode(unsigned int keyval,
                                                   int flags) {
  DCHECK(composition_mode_ == KEY_SEQUENCE_MODE);
  compose_buffer_.push_back(keyval);

  if (compose_buffer_.size() == 2U) {
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kBlackListedDeadKeys); ++i) {
      if (compose_buffer_[0] == kBlackListedDeadKeys[i].first_key &&
          compose_buffer_[1] == kBlackListedDeadKeys[i].second_key ) {
        Reset();
        composed_character_.push_back(kBlackListedDeadKeys[i].output_char);
        return kBlackListedDeadKeys[i].consume;
      }
    }
  }

  // Check compose table.
  uint32 composed_character_utf32 = 0;
  if (CheckCharacterComposeTable(compose_buffer_, &composed_character_utf32)) {
    // Key press is recognized as a part of composition.
    if (composed_character_utf32 != 0) {
      // We get a composed character.
      compose_buffer_.clear();
      UTF32CharacterToUTF16(composed_character_utf32, &composed_character_);
    }
    return true;
  }
  // Key press is not a part of composition.
  compose_buffer_.pop_back();  // Remove the keypress added this time.
  if (!compose_buffer_.empty()) {
    compose_buffer_.clear();
    return true;
  }
  return false;
}

bool CharacterComposer::FilterKeyPressHexMode(unsigned int keyval,
                                              unsigned int keycode,
                                              int flags) {
  DCHECK(composition_mode_ == HEX_MODE);
  const size_t kMaxHexSequenceLength = 8;
  int hex_digit = KeyvalToHexDigit(keyval);
  if (hex_digit < 0) {
    // With 101 keyboard, control + shift + 3 produces '#', but a user may
    // have intended to type '3'.  So, if a hexadecimal character was not found,
    // suppose a user is holding shift key (and possibly control key, too) and
    // try a character with modifier keys removed.
    hex_digit = KeycodeToHexDigit(keycode);
  }

  if (keyval == GDK_KEY_Escape) {
    // Cancel composition when ESC is pressed.
    Reset();
  } else if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter ||
             keyval == GDK_KEY_ISO_Enter ||
             keyval == GDK_KEY_space || keyval == GDK_KEY_KP_Space) {
    // Commit the composed character when Enter or space is pressed.
    CommitHex();
  } else if (keyval == GDK_KEY_BackSpace) {
    // Pop back the buffer when Backspace is pressed.
    if (!compose_buffer_.empty()) {
      compose_buffer_.pop_back();
    } else {
      // If there is no character in |compose_buffer_|, cancel composition.
      Reset();
    }
  } else if (hex_digit >= 0 &&
             compose_buffer_.size() < kMaxHexSequenceLength) {
    // Add the key to the buffer if it is a hex digit.
    compose_buffer_.push_back(hex_digit);
  }

  UpdatePreeditStringHexMode();

  return true;
}

void CharacterComposer::CommitHex() {
  DCHECK(composition_mode_ == HEX_MODE);
  uint32 composed_character_utf32 = 0;
  for (size_t i = 0; i != compose_buffer_.size(); ++i) {
    const uint32 digit = compose_buffer_[i];
    DCHECK(0 <= digit && digit < 16);
    composed_character_utf32 <<= 4;
    composed_character_utf32 |= digit;
  }
  Reset();
  UTF32CharacterToUTF16(composed_character_utf32, &composed_character_);
}

void CharacterComposer::UpdatePreeditStringHexMode() {
  if (composition_mode_ != HEX_MODE) {
    preedit_string_.clear();
    return;
  }
  std::string preedit_string_ascii("u");
  for (size_t i = 0; i != compose_buffer_.size(); ++i) {
    const int digit = compose_buffer_[i];
    DCHECK(0 <= digit && digit < 16);
    preedit_string_ascii += digit <= 9 ? ('0' + digit) : ('a' + (digit - 10));
  }
  preedit_string_ = base::ASCIIToUTF16(preedit_string_ascii);
}

}  // namespace ui
