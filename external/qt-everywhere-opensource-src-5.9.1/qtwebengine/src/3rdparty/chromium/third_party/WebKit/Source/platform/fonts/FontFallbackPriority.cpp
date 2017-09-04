// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/FontFallbackPriority.h"

namespace blink {

bool isNonTextFallbackPriority(FontFallbackPriority fallbackPriority) {
  return fallbackPriority == FontFallbackPriority::EmojiText ||
         fallbackPriority == FontFallbackPriority::EmojiEmoji;
};

}  // namespace blink
