// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TouchEventManager_h
#define TouchEventManager_h

#include "core/CoreExport.h"
#include "core/events/PointerEventFactory.h"
#include "platform/UserGestureIndicator.h"
#include "public/platform/WebInputEventResult.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"


namespace blink {

class LocalFrame;
class Document;
class PlatformTouchEvent;

// This class takes care of dispatching all touch events and
// maintaining related states.
class CORE_EXPORT TouchEventManager: public UserGestureUtilizedCallback {
    WTF_MAKE_NONCOPYABLE(TouchEventManager);
    DISALLOW_NEW();
public:
    class TouchInfo {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    public:
        DEFINE_INLINE_TRACE()
        {
            visitor->trace(touchNode);
            visitor->trace(targetFrame);
        }

        PlatformTouchPoint point;
        Member<Node> touchNode;
        Member<LocalFrame> targetFrame;
        FloatPoint contentPoint;
        FloatSize adjustedRadius;
        bool knownTarget;
        String region;
    };

    explicit TouchEventManager(LocalFrame*);
    ~TouchEventManager();
    DECLARE_TRACE();

    // Does the hit-testing again if the original hit test result was not inside
    // capturing frame for touch events. Returns true if touch events could be
    // dispatched and otherwise returns false.
    bool reHitTestTouchPointsIfNeeded(
        const PlatformTouchEvent&,
        HeapVector<TouchInfo>&);

    // The TouchInfo array is reference just to prevent the copy. However, it
    // cannot be const as this function might change some of the properties in
    // TouchInfo objects.
    WebInputEventResult handleTouchEvent(
        const PlatformTouchEvent&,
        HeapVector<TouchInfo>&);

    // Resets the internal state of this object.
    void clear();

    // Returns whether there is any touch on the screen.
    bool isAnyTouchActive() const;

    // Indicate that a touch scroll has started.
    void setTouchScrollStarted() { m_touchScrollStarted = true; }

    // Invoked when a UserGestureIndicator corresponding to a touch event is utilized.
    void userGestureUtilized() override;

private:
    void updateTargetAndRegionMapsForTouchStarts(HeapVector<TouchInfo>&);
    void setAllPropertiesOfTouchInfos(HeapVector<TouchInfo>&);

    WebInputEventResult dispatchTouchEvents(
        const PlatformTouchEvent&,
        const HeapVector<TouchInfo>&,
        bool allTouchesReleased);


    // NOTE: If adding a new field to this class please ensure that it is
    // cleared in |TouchEventManager::clear()|.

    const Member<LocalFrame> m_frame;

    // The target of each active touch point indexed by the touch ID.
    using TouchTargetMap = HeapHashMap<unsigned, Member<Node>, DefaultHash<unsigned>::Hash, WTF::UnsignedWithZeroKeyHashTraits<unsigned>>;
    TouchTargetMap m_targetForTouchID;
    using TouchRegionMap = HashMap<unsigned, String, DefaultHash<unsigned>::Hash, WTF::UnsignedWithZeroKeyHashTraits<unsigned>>;
    TouchRegionMap m_regionForTouchID;

    // If set, the document of the active touch sequence. Unset if no touch sequence active.
    Member<Document> m_touchSequenceDocument;

    RefPtr<UserGestureToken> m_touchSequenceUserGestureToken;
    bool m_touchPressed;
    // True if waiting on first touch move after a touch start.
    bool m_waitingForFirstTouchMove;
    // True if a touch is active but scrolling/zooming has started.
    bool m_touchScrollStarted;
    // The touch event currently being handled or NoType if none.
    PlatformEvent::EventType m_currentEvent;
};

} // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::TouchEventManager::TouchInfo);

#endif // TouchEventManager_h
