// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleDifference_h
#define StyleDifference_h

#include "wtf/Allocator.h"
#include "wtf/Assertions.h"

namespace blink {

class StyleDifference {
    STACK_ALLOCATED();
public:
    enum PropertyDifference {
        TransformChanged = 1 << 0,
        OpacityChanged = 1 << 1,
        ZIndexChanged = 1 << 2,
        FilterChanged = 1 << 3,
        BackdropFilterChanged = 1 << 4,
        // The object needs to issue paint invalidations if it is affected by text decorations or properties dependent on color (e.g., border or outline).
        TextDecorationOrColorChanged = 1 << 5,
        // If you add a value here, be sure to update the number of bits on m_propertySpecificDifferences.
    };

    StyleDifference()
        : m_paintInvalidationType(NoPaintInvalidation)
        , m_layoutType(NoLayout)
        , m_recomputeOverflow(false)
        , m_propertySpecificDifferences(0)
    { }

    bool hasDifference() const { return m_paintInvalidationType || m_layoutType || m_propertySpecificDifferences; }

    bool hasAtMostPropertySpecificDifferences(unsigned propertyDifferences) const
    {
        return !m_paintInvalidationType && !m_layoutType && !(m_propertySpecificDifferences & ~propertyDifferences);
    }

    bool needsPaintInvalidation() const { return m_paintInvalidationType != NoPaintInvalidation; }
    void clearNeedsPaintInvalidation() { m_paintInvalidationType = NoPaintInvalidation; }

    // The object just needs to issue paint invalidations.
    bool needsPaintInvalidationObject() const { return m_paintInvalidationType == PaintInvalidationObject; }
    void setNeedsPaintInvalidationObject()
    {
        ASSERT(!needsPaintInvalidationSubtree());
        m_paintInvalidationType = PaintInvalidationObject;
    }

    // The object and its descendants need to issue paint invalidations.
    bool needsPaintInvalidationSubtree() const { return m_paintInvalidationType == PaintInvalidationSubtree; }
    void setNeedsPaintInvalidationSubtree() { m_paintInvalidationType = PaintInvalidationSubtree; }

    bool needsLayout() const { return m_layoutType != NoLayout; }
    void clearNeedsLayout() { m_layoutType = NoLayout; }

    // The offset of this positioned object has been updated.
    bool needsPositionedMovementLayout() const { return m_layoutType == PositionedMovement; }
    void setNeedsPositionedMovementLayout()
    {
        ASSERT(!needsFullLayout());
        m_layoutType = PositionedMovement;
    }

    bool needsFullLayout() const { return m_layoutType == FullLayout; }
    void setNeedsFullLayout() { m_layoutType = FullLayout; }

    bool needsRecomputeOverflow() const { return m_recomputeOverflow; }
    void setNeedsRecomputeOverflow() { m_recomputeOverflow = true; }

    bool transformChanged() const { return m_propertySpecificDifferences & TransformChanged; }
    void setTransformChanged() { m_propertySpecificDifferences |= TransformChanged; }

    bool opacityChanged() const { return m_propertySpecificDifferences & OpacityChanged; }
    void setOpacityChanged() { m_propertySpecificDifferences |= OpacityChanged; }

    bool zIndexChanged() const { return m_propertySpecificDifferences & ZIndexChanged; }
    void setZIndexChanged() { m_propertySpecificDifferences |= ZIndexChanged; }

    bool filterChanged() const { return m_propertySpecificDifferences & FilterChanged; }
    void setFilterChanged() { m_propertySpecificDifferences |= FilterChanged; }

    bool backdropFilterChanged() const { return m_propertySpecificDifferences & BackdropFilterChanged; }
    void setBackdropFilterChanged() { m_propertySpecificDifferences |= BackdropFilterChanged; }

    bool textDecorationOrColorChanged() const { return m_propertySpecificDifferences & TextDecorationOrColorChanged; }
    void setTextDecorationOrColorChanged() { m_propertySpecificDifferences |= TextDecorationOrColorChanged; }

private:
    enum PaintInvalidationType {
        NoPaintInvalidation = 0,
        PaintInvalidationObject,
        PaintInvalidationSubtree
    };
    unsigned m_paintInvalidationType : 2;

    enum LayoutType {
        NoLayout = 0,
        PositionedMovement,
        FullLayout
    };
    unsigned m_layoutType : 2;
    unsigned m_recomputeOverflow : 1;
    unsigned m_propertySpecificDifferences : 6;
};

} // namespace blink

#endif // StyleDifference_h
