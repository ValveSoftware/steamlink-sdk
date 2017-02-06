// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ColorPropertyFunctions_h
#define ColorPropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "core/css/StyleColor.h"

namespace blink {

class ComputedStyle;

class ColorPropertyFunctions {
public:
    static StyleColor getInitialColor(CSSPropertyID);
    static StyleColor getUnvisitedColor(CSSPropertyID, const ComputedStyle&);
    static StyleColor getVisitedColor(CSSPropertyID, const ComputedStyle&);
    static void setUnvisitedColor(CSSPropertyID, ComputedStyle&, const Color&);
    static void setVisitedColor(CSSPropertyID, ComputedStyle&, const Color&);
};

} // namespace blink

#endif // ColorPropertyFunctions_h
