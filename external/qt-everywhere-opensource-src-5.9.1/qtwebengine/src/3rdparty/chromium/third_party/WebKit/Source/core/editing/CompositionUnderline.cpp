// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/CompositionUnderline.h"

namespace blink {

CompositionUnderline::CompositionUnderline(unsigned startOffset,
                                           unsigned endOffset,
                                           const Color& color,
                                           bool thick,
                                           const Color& backgroundColor)
    : m_color(color), m_thick(thick), m_backgroundColor(backgroundColor) {
  // Sanitize offsets by ensuring a valid range corresponding to the last
  // possible position.
  // TODO(wkorman): Consider replacing with DCHECK_LT(startOffset, endOffset).
  m_startOffset =
      std::min(startOffset, std::numeric_limits<unsigned>::max() - 1u);
  m_endOffset = std::max(m_startOffset + 1u, endOffset);
}

}  // namespace blink
