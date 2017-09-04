// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ScrollbarManager.h"

namespace blink {

ScrollbarManager::ScrollbarManager(ScrollableArea& scrollableArea)
    : m_scrollableArea(&scrollableArea),
      m_hBarIsAttached(0),
      m_vBarIsAttached(0) {}

DEFINE_TRACE(ScrollbarManager) {
  visitor->trace(m_scrollableArea);
  visitor->trace(m_hBar);
  visitor->trace(m_vBar);
}

void ScrollbarManager::dispose() {
  m_hBarIsAttached = m_vBarIsAttached = 0;
  destroyScrollbar(HorizontalScrollbar);
  destroyScrollbar(VerticalScrollbar);
}

}  // namespace blink
