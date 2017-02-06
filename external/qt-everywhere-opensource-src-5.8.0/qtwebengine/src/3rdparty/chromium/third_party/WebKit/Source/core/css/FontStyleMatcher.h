// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontStyleMatcher_h
#define FontStyleMatcher_h

#include "platform/fonts/FontTraits.h"

namespace blink {

class CSSSegmentedFontFace;

class FontStyleMatcher final {
public:
    explicit FontStyleMatcher(const FontTraits& fontTraits) : m_fontTraits(fontTraits) {};
    bool isCandidateBetter(CSSSegmentedFontFace* candidate, CSSSegmentedFontFace* current);

private:
    FontStyleMatcher();
    const FontTraits& m_fontTraits;
};

} // namespace blink

#endif
