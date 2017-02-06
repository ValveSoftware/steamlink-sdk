// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CharacterRange_h
#define CharacterRange_h

namespace blink {

struct CharacterRange {
    CharacterRange(float from, float to) : start(from), end(to)
    {
        ASSERT(start <= end);
    }

    float width() const { return end - start; }

    float start;
    float end;
};

} // namespace blink

#endif // CharacterRange_h
