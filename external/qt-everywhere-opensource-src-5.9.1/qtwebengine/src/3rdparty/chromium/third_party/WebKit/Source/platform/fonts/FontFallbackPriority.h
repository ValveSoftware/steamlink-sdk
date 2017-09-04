// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontFallbackPriority_h
#define FontFallbackPriority_h

namespace blink {

// http://unicode.org/reports/tr51/#Presentation_Style discusses the differences
// between emoji in text and the emoji in emoji presentation. In that sense, the
// EmojiEmoji wording is taken from there.  Also compare
// http://unicode.org/Public/emoji/1.0/emoji-data.txt
enum class FontFallbackPriority {
  // For regular non-symbols text,
  // normal text fallback in FontFallbackIterator
  Text,
  // For emoji in text presentaiton
  EmojiText,
  // For emoji in emoji presentation
  EmojiEmoji,
  Invalid
};

bool isNonTextFallbackPriority(FontFallbackPriority);

};  // namespace blink

#endif
