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
class LayoutObject;
class LayoutRect;

class BackgroundImageGeometry {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    BackgroundImageGeometry()
        : m_hasNonLocalGeometry(false)
    { }

    void calculate(const LayoutBoxModelObject&, const LayoutBoxModelObject* paintContainer,
        const GlobalPaintFlags, const FillLayer&, const LayoutRect& paintRect);

    const LayoutRect& destRect() const { return m_destRect; }
    const LayoutSize& tileSize() const { return m_tileSize; }
    const LayoutPoint& phase() const { return m_phase; }
    // Space-size represents extra width and height that may be added to
    // the image if used as a pattern with background-repeat: space.
    const LayoutSize& spaceSize() const { return m_repeatSpacing; }
    // Has background-attachment: fixed. Implies that we can't always cheaply compute destRect.
    bool hasNonLocalGeometry() const { return m_hasNonLocalGeometry; }

private:
    void setDestRect(const LayoutRect& destRect) { m_destRect = destRect; }
    void setPhase(const LayoutPoint& phase) { m_phase = phase; }
    void setTileSize(const LayoutSize& tileSize) { m_tileSize = tileSize; }
    void setSpaceSize(const LayoutSize& repeatSpacing) { m_repeatSpacing = repeatSpacing; }
    void setPhaseX(LayoutUnit x) { m_phase.setX(x); }
    void setPhaseY(LayoutUnit y) { m_phase.setY(y); }
    void setNoRepeatX(LayoutUnit xOffset);
    void setNoRepeatY(LayoutUnit yOffset);
    void setRepeatX(const FillLayer&, LayoutUnit, LayoutUnit, LayoutUnit, LayoutUnit);
    void setRepeatY(const FillLayer&, LayoutUnit, LayoutUnit, LayoutUnit, LayoutUnit);
    void setSpaceX(LayoutUnit, LayoutUnit, LayoutUnit);
    void setSpaceY(LayoutUnit, LayoutUnit, LayoutUnit);

    void useFixedAttachment(const LayoutPoint& attachmentPoint);
    void setHasNonLocalGeometry() { m_hasNonLocalGeometry = true; }

    LayoutRect m_destRect;
    LayoutPoint m_phase;
    LayoutSize m_tileSize;
    LayoutSize m_repeatSpacing;
    bool m_hasNonLocalGeometry;
};

} // namespace blink

#endif // BackgroundImageGeometry_h
