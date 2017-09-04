// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintPropertyFunctions_h
#define PaintPropertyFunctions_h

#include "core/CSSPropertyNames.h"

namespace blink {

class ComputedStyle;
class StyleColor;
class Color;

class PaintPropertyFunctions {
 public:
  static bool getInitialColor(CSSPropertyID, StyleColor& result);
  static bool getColor(CSSPropertyID, const ComputedStyle&, StyleColor& result);
  static void setColor(CSSPropertyID, ComputedStyle&, const Color&);
};

}  // namespace blink

#endif  // PaintPropertyFunctions_h
