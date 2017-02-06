// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthPropertyFunctions_h
#define LengthPropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "platform/Length.h"
#include "wtf/Allocator.h"

namespace blink {

class ComputedStyle;

class LengthPropertyFunctions {
    STATIC_ONLY(LengthPropertyFunctions);
public:
    static ValueRange getValueRange(CSSPropertyID);
    static bool isZoomedLength(CSSPropertyID);
    static bool getPixelsForKeyword(CSSPropertyID, CSSValueID, double& resultPixels);
    static bool getInitialLength(CSSPropertyID, Length& result);
    static bool getLength(CSSPropertyID, const ComputedStyle&, Length& result);
    static bool setLength(CSSPropertyID, ComputedStyle&, const Length&);
};

} // namespace blink

#endif // LengthPropertyFunctions_h
