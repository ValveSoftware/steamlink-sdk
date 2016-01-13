// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_TEXT_CONSTANTS_H_
#define UI_GFX_TEXT_CONSTANTS_H_

namespace gfx {

// TODO(msw): Distinguish between logical character stops and glyph stops?
// TODO(msw): Merge with base::i18n::BreakIterator::BreakType.
enum BreakType {
  CHARACTER_BREAK = 0, // Stop cursor movement on neighboring characters.
  WORD_BREAK,          // Stop cursor movement on nearest word boundaries.
  LINE_BREAK,          // Stop cursor movement on line ends as shown on screen.
};

// Horizontal text alignment modes.
enum HorizontalAlignment {
  ALIGN_LEFT = 0, // Align the text's left edge with that of its display area.
  ALIGN_CENTER,   // Align the text's center with that of its display area.
  ALIGN_RIGHT,    // Align the text's right edge with that of its display area.
  ALIGN_TO_HEAD,  // Align the text to its first strong character's direction.
};

// The directionality modes used to determine the base text direction.
enum DirectionalityMode {
  DIRECTIONALITY_FROM_TEXT = 0, // Use the first strong character's direction.
  DIRECTIONALITY_FROM_UI,       // Use the UI locale's text reading direction.
  DIRECTIONALITY_FORCE_LTR,     // Use LTR regardless of content or UI locale.
  DIRECTIONALITY_FORCE_RTL,     // Use RTL regardless of content or UI locale.
};

// Text styles and adornments.
// TODO(msw): Merge with gfx::Font::FontStyle.
enum TextStyle {
  BOLD = 0,
  ITALIC,
  STRIKE,
  DIAGONAL_STRIKE,
  UNDERLINE,
  NUM_TEXT_STYLES,
};

// Elision behaviors of text that exceeds constrained dimensions.
enum ElideBehavior {
  TRUNCATE = 0, // Do not elide or fade; the text may be truncated at the end.
  ELIDE_HEAD,   // Add an ellipsis at the start of the string.
  ELIDE_MIDDLE, // Add an ellipsis in the middle of the string.
  ELIDE_TAIL,   // Add an ellipsis at the end of the string.
  ELIDE_EMAIL,  // Add ellipses to username and domain substrings.
  FADE_TAIL,    // Fade the string's end opposite of its horizontal alignment.
};

}  // namespace gfx

#endif  // UI_GFX_TEXT_CONSTANTS_H_
