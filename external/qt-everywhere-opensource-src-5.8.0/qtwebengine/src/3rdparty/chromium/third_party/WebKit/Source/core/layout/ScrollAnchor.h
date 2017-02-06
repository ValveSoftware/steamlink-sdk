// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollAnchor_h
#define ScrollAnchor_h

#include "core/CoreExport.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/heap/Handle.h"

namespace blink {

class LayoutObject;
class ScrollableArea;

// Scrolls to compensate for layout movements (bit.ly/scroll-anchoring).
class CORE_EXPORT ScrollAnchor final {
    DISALLOW_NEW();

public:
    ScrollAnchor(ScrollableArea*);
    ~ScrollAnchor();

    // The LayoutObject we are currently anchored to. Lazily computed during
    // save() and cached until the next call to clear().
    LayoutObject* anchorObject() const { return m_current.m_anchorObject; }

    // Indicates that the next save() should compute a new anchor. (In certain
    // cases the previous anchor will be reused; see comments in restore.)
    void clear();

    // Records the anchor's location in relation to the scroller. Should be
    // called when the scroller is about to be laid out.
    void save();

    // Scrolls to compensate for any change in the anchor's relative location
    // since the most recent call to save(). Should be called immediately after
    // the scroller has been laid out.
    void restore();

    enum class Corner {
        TopLeft = 0,
        TopRight,
    };
    // Which corner of the anchor object we are currently anchored to.
    // Only meaningful if anchorObject() is non-null.
    Corner corner() const { return m_current.m_corner; }

    // Checks if we hold any references to the specified object.
    bool refersTo(const LayoutObject*) const;

    // Notifies us that an object will be removed from the layout tree.
    void notifyRemoved(LayoutObject*);

    DEFINE_INLINE_TRACE() { visitor->trace(m_scroller); }

private:
    void findAnchor();

    enum WalkStatus {
        Skip = 0,
        Constrain,
        Continue,
        Return
    };
    struct ExamineResult {
        ExamineResult(WalkStatus s)
            : status(s)
            , viable(false)
            , corner(Corner::TopLeft) {}

        ExamineResult(WalkStatus s, Corner c)
            : status(s)
            , viable(true)
            , corner(c) {}

        WalkStatus status;
        bool viable;
        Corner corner;
    };
    ExamineResult examine(const LayoutObject*) const;

    struct AnchorPoint {
        AnchorPoint()
            : m_anchorObject(nullptr)
            , m_corner(Corner::TopLeft) {}

        AnchorPoint(LayoutObject* anchorObject, Corner corner)
            : m_anchorObject(anchorObject)
            , m_corner(corner) {}

        explicit operator bool() const { return m_anchorObject; }

        void clear();

        // The LayoutObject we should anchor to.
        LayoutObject* m_anchorObject;

        // Which corner of m_anchorObject's bounding box to anchor to.
        Corner m_corner;

        // Location of m_layoutObject relative to scroller at time of save().
        LayoutPoint m_savedRelativeOffset;
    };
    IntSize computeAdjustment(const AnchorPoint&) const;
    void adjust(IntSize);

    // The scroller that owns and is adjusted by this ScrollAnchor.
    Member<ScrollableArea> m_scroller;

    // The current anchor point. Lazily computed.
    AnchorPoint m_current;

    // The anchor point that was used for the most recent non-zero adjustment.
    AnchorPoint m_lastAdjusted;

    // The size of the most recent non-zero adjustment.
    IntSize m_lastAdjustment;

    // True iff the last adjustment was the exact opposite of the one before it.
    // A bounce suggests a circular interaction with a scroll event handler.
    bool m_hasBounced;
};

} // namespace blink

#endif // ScrollAnchor_h
