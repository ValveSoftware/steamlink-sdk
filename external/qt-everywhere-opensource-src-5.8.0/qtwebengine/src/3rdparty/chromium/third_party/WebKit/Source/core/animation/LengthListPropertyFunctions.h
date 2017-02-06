// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthListPropertyFunctions_h
#define LengthListPropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "platform/Length.h"
#include "wtf/Vector.h"

namespace blink {

class ComputedStyle;

class LengthListPropertyFunctions {
    STATIC_ONLY(LengthListPropertyFunctions);
public:
    static ValueRange getValueRange(CSSPropertyID);
    static bool getInitialLengthList(CSSPropertyID, Vector<Length>& result);
    static bool getLengthList(CSSPropertyID, const ComputedStyle&, Vector<Length>& result);
    static void setLengthList(CSSPropertyID, ComputedStyle&, Vector<Length>&& lengthList);
};

} // namespace blink

#endif // LengthListPropertyFunctions_h
