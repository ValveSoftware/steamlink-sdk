// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SPELLCHECK_RESULT_H_
#define CHROME_COMMON_SPELLCHECK_RESULT_H_

#include <stdint.h>

#include "base/strings/string16.h"

// This class mirrors blink::WebTextCheckingResult which holds a
// misspelled range inside the checked text. It also contains a
// possible replacement of the misspelling if it is available.
struct SpellCheckResult {
  enum Decoration {
    // Red underline for misspelled words.
    SPELLING = 1 << 1,

    // Gray underline for correctly spelled words that are incorrectly used in
    // their context.
    GRAMMAR = 1 << 2,

    // No underline for words that spellcheck needs to track. For example, a
    // word in the custom spellcheck dictionary.
    INVISIBLE = 1 << 3,
  };

  explicit SpellCheckResult(Decoration d = SPELLING,
                            int loc = 0,
                            int len = 0,
                            const base::string16& rep = base::string16(),
                            uint32_t h = 0)
      : decoration(d), location(loc), length(len), replacement(rep), hash(h) {}

  Decoration decoration;
  int location;
  int length;
  base::string16 replacement;
  uint32_t hash;
};

#endif  // CHROME_COMMON_SPELLCHECK_RESULT_H_
