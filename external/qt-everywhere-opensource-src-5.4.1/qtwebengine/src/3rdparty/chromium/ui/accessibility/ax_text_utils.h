// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TEXT_UTILS_H_
#define UI_ACCESSIBILITY_AX_TEXT_UTILS_H_

#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "ui/accessibility/ax_export.h"

namespace ui {

// Boundaries that can be passed to FindAccessibleTextBoundary,
// representing various visual boundaries in (potentially multi-line)
// text. This is used by assistive technology in order to, for example,
// retrieve the nearest word to the cursor, or retrieve all of the
// text from the current cursor position to the end of the line.
// These should be self-explanatory; "line" here refers to the visual
// line as currently displayed (possibly affected by wrapping).
enum TextBoundaryType {
  CHAR_BOUNDARY,
  WORD_BOUNDARY,
  LINE_BOUNDARY,
  SENTENCE_BOUNDARY,
  PARAGRAPH_BOUNDARY,
  ALL_BOUNDARY
};

// A direction when searching for the next boundary.
enum TextBoundaryDirection {
  // Search forwards for the next boundary past the starting position.
  FORWARDS_DIRECTION,
  // Search backwards for the previous boundary before the starting position.
  BACKWARDS_DIRECTION
};

// Convenience method needed to implement platform-specific text
// accessibility APIs like IAccessible2. Search forwards or backwards
// (depending on |direction|) from the given |start_offset| until the
// given boundary is found, and return the offset of that boundary,
// using the vector of line break character offsets in |line_breaks|.
size_t AX_EXPORT
    FindAccessibleTextBoundary(const base::string16& text,
                               const std::vector<int>& line_breaks,
                               TextBoundaryType boundary,
                               size_t start_offset,
                               TextBoundaryDirection direction);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TEXT_UTILS_H_
