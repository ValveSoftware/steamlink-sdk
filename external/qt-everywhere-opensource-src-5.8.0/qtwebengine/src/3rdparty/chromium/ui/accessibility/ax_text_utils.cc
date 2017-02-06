// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_text_utils.h"

#include "base/i18n/break_iterator.h"
#include "base/logging.h"
#include "base/strings/string_util.h"

namespace ui {

// line_breaks is a Misnomer. Blink provides the start offsets of each line
// not the line breaks.
// TODO(nektar): Rename line_breaks a11y attribute and variable references.
size_t FindAccessibleTextBoundary(const base::string16& text,
                                  const std::vector<int>& line_breaks,
                                  TextBoundaryType boundary,
                                  size_t start_offset,
                                  TextBoundaryDirection direction) {
  size_t text_size = text.size();
  DCHECK_LE(start_offset, text_size);

  if (boundary == CHAR_BOUNDARY) {
    if (direction == FORWARDS_DIRECTION && start_offset < text_size)
      return start_offset + 1;
    else
      return start_offset;
  }

  base::i18n::BreakIterator word_iter(text,
                                      base::i18n::BreakIterator::BREAK_WORD);
  if (boundary == WORD_BOUNDARY) {
    if (!word_iter.Init())
      return start_offset;
  }

  if (boundary == LINE_BOUNDARY) {
    if (direction == FORWARDS_DIRECTION) {
      for (size_t j = 0; j < line_breaks.size(); ++j) {
          size_t line_break = line_breaks[j] >= 0 ? line_breaks[j] : 0;
        if (line_break > start_offset)
          return line_break;
      }
      return text_size;
    } else {
      for (size_t j = line_breaks.size(); j != 0; --j) {
        size_t line_break = line_breaks[j - 1] >= 0 ? line_breaks[j - 1] : 0;
        if (line_break <= start_offset)
          return line_break;
      }
      return 0;
    }
  }

  size_t result = start_offset;
  for (;;) {
    size_t pos;
    if (direction == FORWARDS_DIRECTION) {
      if (result >= text_size)
        return text_size;
      pos = result;
    } else {
      if (result == 0)
        return 0;
      pos = result - 1;
    }

    switch (boundary) {
      case CHAR_BOUNDARY:
      case LINE_BOUNDARY:
        NOTREACHED();  // These are handled above.
        break;
      case WORD_BOUNDARY:
        if (word_iter.IsStartOfWord(result)) {
          // If we are searching forward and we are still at the start offset,
          // we need to find the next word.
          if (direction == BACKWARDS_DIRECTION || result != start_offset)
            return result;
        }
        break;
      case PARAGRAPH_BOUNDARY:
        if (text[pos] == '\n')
          return result;
        break;
      case SENTENCE_BOUNDARY:
        if ((text[pos] == '.' || text[pos] == '!' || text[pos] == '?') &&
            (pos == text_size - 1 ||
             base::IsUnicodeWhitespace(text[pos + 1]))) {
          return result;
        }
        break;
      case ALL_BOUNDARY:
      default:
        break;
    }

    if (direction == FORWARDS_DIRECTION) {
      result++;
    } else {
      result--;
    }
  }
}

}  // namespace ui
