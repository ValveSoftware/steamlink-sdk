// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/MockHyphenation.h"

namespace blink {

size_t MockHyphenation::lastHyphenLocation(const StringView& text,
    size_t beforeIndex) const
{
    String str = text.toString();
    if (str.endsWith("phenation", TextCaseASCIIInsensitive)) {
        if (beforeIndex - (str.length() - 9) > 4)
            return 4 + (str.length() - 9);
        if (str.endsWith("hyphenation", TextCaseASCIIInsensitive)
            && beforeIndex - (str.length() - 11) > 2) {
            return 2 + (str.length() - 11);
        }
    }
    return 0;
}

} // namespace blink
