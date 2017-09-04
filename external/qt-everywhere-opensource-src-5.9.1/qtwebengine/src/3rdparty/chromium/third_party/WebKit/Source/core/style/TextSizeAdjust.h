// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextSizeAdjust_h
#define TextSizeAdjust_h

#include "wtf/Allocator.h"

namespace blink {

// Value for text-size-adjust, see: https://drafts.csswg.org/css-size-adjust
class TextSizeAdjust {
  DISALLOW_NEW();

 public:
  TextSizeAdjust(float adjustment) : m_adjustment(adjustment) {}

  // Negative values are invalid so we use them internally to signify 'auto'.
  static TextSizeAdjust adjustAuto() { return TextSizeAdjust(-1); }
  // An adjustment of 'none' is equivalent to 100%.
  static TextSizeAdjust adjustNone() { return TextSizeAdjust(1); }

  bool isAuto() const { return m_adjustment < 0.f; }

  float multiplier() const {
    // If the adjustment is 'auto', no multiplier is available.
    DCHECK(!isAuto());
    return m_adjustment;
  }

  bool operator==(const TextSizeAdjust& o) const {
    return m_adjustment == o.m_adjustment;
  }

  bool operator!=(const TextSizeAdjust& o) const { return !(*this == o); }

 private:
  // Percent adjustment, without units (i.e., 10% is .1 and not 10). Negative
  // values indicate 'auto' adjustment.
  float m_adjustment;
};

}  // namespace blink

#endif  // TextSizeAdjust_h
