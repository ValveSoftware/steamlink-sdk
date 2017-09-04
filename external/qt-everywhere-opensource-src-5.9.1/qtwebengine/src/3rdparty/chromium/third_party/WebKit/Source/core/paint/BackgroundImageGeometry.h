// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundImageGeometry_h
#define BackgroundImageGeometry_h

#include "core/paint/PaintPhase.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/geometry/LayoutSize.h"
#include "wtf/Allocator.h"

namespace blink {

class FillLayer;
class LayoutBoxModelObject;
class LayoutRect;

class BackgroundImageGeometry {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  BackgroundImageGeometry() : m_hasNonLocalGeometry(false) {}

  void calculate(const LayoutBoxModelObject&,
                 const LayoutBoxModelObject* paintContainer,
                 const GlobalPaintFlags,
                 const FillLayer&,
                 const LayoutRect& paintRect);

  // destRect() is the rect in the space of the containing box into which to
  // draw the image.
  const LayoutRect& destRect() const { return m_destRect; }
  // If the image is repeated via tiling, tileSize() is the size
  // in pixels of the area into which to draw the entire image once.
  //
  // tileSize() need not be the same as the intrinsic size of the image; if not,
  // it means the image will be resized (via an image filter) when painted into
  // that tile region. This may happen because of CSS background-size and
  // background-repeat requirements.
  const LayoutSize& tileSize() const { return m_tileSize; }
  // phase() represents the point in the image that will appear at (0,0) in the
  // destination space. The point is defined in tileSize() coordinates.
  const LayoutPoint& phase() const { return m_phase; }
  // Space-size represents extra width and height that may be added to
  // the image if used as a pattern with background-repeat: space.
  const LayoutSize& spaceSize() const { return m_repeatSpacing; }
  // Has background-attachment: fixed. Implies that we can't always cheaply
  // compute destRect.
  bool hasNonLocalGeometry() const { return m_hasNonLocalGeometry; }

 private:
  void setDestRect(const LayoutRect& destRect) { m_destRect = destRect; }
  void setPhase(const LayoutPoint& phase) { m_phase = phase; }
  void setTileSize(const LayoutSize& tileSize) { m_tileSize = tileSize; }
  void setSpaceSize(const LayoutSize& repeatSpacing) {
    m_repeatSpacing = repeatSpacing;
  }
  void setPhaseX(LayoutUnit x) { m_phase.setX(x); }
  void setPhaseY(LayoutUnit y) { m_phase.setY(y); }

  void setNoRepeatX(LayoutUnit xOffset);
  void setNoRepeatY(LayoutUnit yOffset);
  void setRepeatX(const FillLayer&,
                  LayoutUnit,
                  LayoutUnit,
                  LayoutUnit,
                  LayoutUnit);
  void setRepeatY(const FillLayer&,
                  LayoutUnit,
                  LayoutUnit,
                  LayoutUnit,
                  LayoutUnit);
  void setSpaceX(LayoutUnit, LayoutUnit, LayoutUnit);
  void setSpaceY(LayoutUnit, LayoutUnit, LayoutUnit);

  void useFixedAttachment(const LayoutPoint& attachmentPoint);
  void setHasNonLocalGeometry() { m_hasNonLocalGeometry = true; }

  // TODO(schenney): Convert these to IntPoints for values that we snap
  LayoutRect m_destRect;
  LayoutPoint m_phase;
  LayoutSize m_tileSize;
  LayoutSize m_repeatSpacing;
  bool m_hasNonLocalGeometry;
};

}  // namespace blink

#endif  // BackgroundImageGeometry_h
