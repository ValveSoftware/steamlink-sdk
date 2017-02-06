// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollManager_h
#define ScrollManager_h

#include "core/CoreExport.h"
#include "core/page/EventWithHitTestResults.h"
#include "platform/PlatformEvent.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Visitor.h"
#include "platform/scroll/ScrollTypes.h"
#include "public/platform/WebInputEventResult.h"
#include "wtf/Allocator.h"
#include <deque>

namespace blink {

class AutoscrollController;
class FloatPoint;
class FrameHost;
class LayoutBox;
class LayoutObject;
class LocalFrame;
class PaintLayer;
class PaintLayerScrollableArea;
class PlatformGestureEvent;
class Scrollbar;
class ScrollState;

// This class takes care of scrolling and resizing and the related states. The
// user action that causes scrolling or resizing is determined in other *Manager
// classes and they call into this class for doing the work.
class CORE_EXPORT ScrollManager {
    WTF_MAKE_NONCOPYABLE(ScrollManager);
    DISALLOW_NEW();
public:
    explicit ScrollManager(LocalFrame*);
    ~ScrollManager();
    DECLARE_TRACE();

    void clear();

    bool panScrollInProgress() const;
    AutoscrollController* autoscrollController() const;
    void stopAutoscroll();

    // Performs a chaining logical scroll, within a *single* frame, starting
    // from either a provided starting node or a default based on the focused or
    // most recently clicked node, falling back to the frame.
    // Returns true if the scroll was consumed.
    // direction - The logical direction to scroll in. This will be converted to
    //             a physical direction for each LayoutBox we try to scroll
    //             based on that box's writing mode.
    // granularity - The units that the  scroll delta parameter is in.
    // startNode - Optional. If provided, start chaining from the given node.
    //             If not, use the current focus or last clicked node.
    bool logicalScroll(ScrollDirection, ScrollGranularity, Node* startNode, Node* mousePressNode);

    // Performs a logical scroll that chains, crossing frames, starting from
    // the given node or a reasonable default (focus/last clicked).
    bool bubblingScroll(ScrollDirection, ScrollGranularity, Node* startingNode, Node* mousePressNode);

    void setFrameWasScrolledByUser();


    // TODO(crbug.com/616491): Consider moving all gesture related functions to
    // another class.

    // Handle the provided scroll gesture event, propagating down to child frames as necessary.
    WebInputEventResult handleGestureScrollEvent(const PlatformGestureEvent&);

    WebInputEventResult handleGestureScrollEnd(const PlatformGestureEvent&);

    bool isScrollbarHandlingGestures() const;

    // Returns true if the gesture event should be handled in ScrollManager.
    bool canHandleGestureEvent(const GestureEventWithHitTestResults&);

    // These functions are related to |m_resizeScrollableArea|.
    bool inResizeMode() const;
    void resize(const PlatformEvent&);
    // Clears |m_resizeScrollableArea|. if |shouldNotBeNull| is true this
    // function DCHECKs to make sure that variable is indeed not null.
    void clearResizeScrollableArea(bool shouldNotBeNull);
    void setResizeScrollableArea(PaintLayer*, IntPoint);

private:

    WebInputEventResult handleGestureScrollUpdate(const PlatformGestureEvent&);
    WebInputEventResult handleGestureScrollBegin(const PlatformGestureEvent&);

    WebInputEventResult passScrollGestureEventToWidget(const PlatformGestureEvent&, LayoutObject*);

    void clearGestureScrollState();

    void customizedScroll(const Node& startNode, ScrollState&);

    FrameHost* frameHost() const;

    bool isEffectiveRootScroller(const Node&) const;

    bool handleScrollGestureOnResizer(Node*, const PlatformGestureEvent&);

    void recomputeScrollChain(const Node& startNode,
        std::deque<int>& scrollChain);


    // NOTE: If adding a new field to this class please ensure that it is
    // cleared in |ScrollManager::clear()|.

    const Member<LocalFrame> m_frame;

    // Only used with the ScrollCustomization runtime enabled feature.
    std::deque<int> m_currentScrollChain;

    Member<Node> m_scrollGestureHandlingNode;

    bool m_lastGestureScrollOverWidget;

    // The most recent element to scroll natively during this scroll
    // sequence. Null if no native element has scrolled this scroll
    // sequence, or if the most recent element to scroll used scroll
    // customization.
    Member<Node> m_previousGestureScrolledNode;

    // True iff some of the delta has been consumed for the current
    // scroll sequence in this frame, or any child frames. Only used
    // with ScrollCustomization. If some delta has been consumed, a
    // scroll which shouldn't propagate can't cause any element to
    // scroll other than the |m_previousGestureScrolledNode|.
    bool m_deltaConsumedForScrollSequence;

    Member<Scrollbar> m_scrollbarHandlingScrollGesture;

    Member<PaintLayerScrollableArea> m_resizeScrollableArea;

    LayoutSize m_offsetFromResizeCorner; // In the coords of m_resizeScrollableArea.

};

} // namespace blink

#endif // ScrollManager_h
