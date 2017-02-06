// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/word_trimmer.h"

#include <algorithm>
#include <vector>

#include "base/i18n/break_iterator.h"

base::string16 TrimWords(size_t* start,
                         size_t end,
                         const base::string16& text,
                         size_t keep) {
  if (*start > text.length() || *start > end)
    return text;
  base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);
  if (!iter.Init())
    return text;
  // A circular buffer of the last |keep + 1| words seen before position |start|
  // in |text|.
  std::vector<size_t> word_offset(keep + 1, 0);
  size_t first = std::string::npos;
  size_t last = std::string::npos;
  while (iter.Advance()) {
    if (iter.IsWord()) {
      word_offset[keep] = iter.prev();
      if ((*start >= iter.prev() && *start < iter.pos()) ||
          (end > iter.prev() && end <= iter.pos())) {
        if (first == std::string::npos)
          first = word_offset[0];
        last = iter.pos();
      }
      if (first == std::string::npos) {
        std::rotate(word_offset.begin(),
                    word_offset.begin() + 1,
                    word_offset.end());
      }
      if (iter.prev() > end && keep) {
        last = iter.pos();
        keep--;
      }
    }
  }
  if (first == std::string::npos)
    return text;
  *start -= first;
  return text.substr(first, last - first);
}
