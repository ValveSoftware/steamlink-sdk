// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/CullRect.h"

#include "platform/geometry/FloatRect.h"
#include "platform/geometry/LayoutRect.h"

namespace blink {

CullRect::CullRect(const CullRect& cullRect, const IntPoint& offset) {
  m_rect = cullRect.m_rect;
  m_rect.moveBy(offset);
}

CullRect::CullRect(const CullRect& cullRect, const IntSize& offset) {
  m_rect = cullRect.m_rect;
  m_rect.move(offset);
}

bool CullRect::intersectsCullRect(const IntRect& boundingBox) const {
  return boundingBox.intersects(m_rect);
}

bool CullRect::intersectsCullRect(const AffineTransform& transform,
                                  const FloatRect& boundingBox) const {
  return transform.mapRect(boundingBox).intersects(m_rect);
}

bool CullRect::intersectsCullRect(const LayoutRect& rectArg) const {
  return m_rect.intersects(enclosingIntRect(rectArg));
}

bool CullRect::intersectsHorizontalRange(LayoutUnit lo, LayoutUnit hi) const {
  return !(lo >= m_rect.maxX() || hi <= m_rect.x());
}

bool CullRect::intersectsVerticalRange(LayoutUnit lo, LayoutUnit hi) const {
  return !(lo >= m_rect.maxY() || hi <= m_rect.y());
}

void CullRect::updateCullRect(const AffineTransform& localToParentTransform) {
  if (m_rect != LayoutRect::infiniteIntRect())
    m_rect = localToParentTransform.inverse().mapRect(m_rect);
}

}  // namespace blink
