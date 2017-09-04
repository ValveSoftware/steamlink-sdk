// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/observer/ResizeObserverEntry.h"

#include "core/dom/ClientRect.h"
#include "core/dom/Element.h"
#include "core/observer/ResizeObservation.h"

namespace blink {

ResizeObserverEntry::ResizeObserverEntry(Element* target,
                                         const LayoutRect& contentRect)
    : m_target(target) {
  m_contentRect = ClientRect::create(FloatRect(
      FloatPoint(contentRect.location()), FloatSize(contentRect.size())));
}

DEFINE_TRACE(ResizeObserverEntry) {
  visitor->trace(m_target);
  visitor->trace(m_contentRect);
}

}  // namespace blink
