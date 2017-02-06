// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShapeResultTestInfo_h
#define ShapeResultTestInfo_h

#include "platform/fonts/shaping/HarfBuzzShaper.h"

#include <hb.h>

namespace blink {

class PLATFORM_EXPORT ShapeResultTestInfo : public ShapeResult {
public:
    unsigned numberOfRunsForTesting() const;
    bool runInfoForTesting(unsigned runIndex, unsigned& startIndex,
        unsigned& numGlyphs, hb_script_t&) const;
    uint16_t glyphForTesting(unsigned runIndex, size_t glyphIndex) const;
    float advanceForTesting(unsigned runIndex, size_t glyphIndex) const;
};

} // namespace blink

#endif // ShapeResultTestInfo_h
