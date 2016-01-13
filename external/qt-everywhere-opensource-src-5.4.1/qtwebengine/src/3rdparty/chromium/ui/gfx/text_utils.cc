// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/text_utils.h"

#include "base/i18n/char_iterator.h"

namespace gfx {

base::string16 RemoveAcceleratorChar(const base::string16& s,
                                     base::char16 accelerator_char,
                                     int* accelerated_char_pos,
                                     int* accelerated_char_span) {
  bool escaped = false;
  ptrdiff_t last_char_pos = -1;
  int last_char_span = 0;
  base::i18n::UTF16CharIterator chars(&s);
  base::string16 accelerator_removed;

  accelerator_removed.reserve(s.size());
  while (!chars.end()) {
    int32 c = chars.get();
    int array_pos = chars.array_pos();
    chars.Advance();

    if (c != accelerator_char || escaped) {
      int span = chars.array_pos() - array_pos;
      if (escaped && c != accelerator_char) {
        last_char_pos = accelerator_removed.size();
        last_char_span = span;
      }
      for (int i = 0; i < span; i++)
        accelerator_removed.push_back(s[array_pos + i]);
      escaped = false;
    } else {
      escaped = true;
    }
  }

  if (accelerated_char_pos)
    *accelerated_char_pos = last_char_pos;
  if (accelerated_char_span)
    *accelerated_char_span = last_char_span;

  return accelerator_removed;
}

}  // namespace gfx
