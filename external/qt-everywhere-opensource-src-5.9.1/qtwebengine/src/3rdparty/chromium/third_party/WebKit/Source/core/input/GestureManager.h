// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GestureManager_h
#define GestureManager_h

#include "core/CoreExport.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/HitTestRequest.h"
#include "core/page/EventWithHitTestResults.h"
#include "platform/PlatformEvent.h"
#include "public/platform/WebInputEventResult.h"

namespace blink {

class ScrollManager;
class SelectionController;
class PointerEventManager;
class MouseEventManager;

// This class takes care of gestures and delegating the action based on the
// gesture to the responsible class.
class CORE_EXPORT GestureManager
    : public GarbageCollectedFinalized<GestureManager> {
  WTF_MAKE_NONCOPYABLE(GestureManager);

 public:
  GestureManager(LocalFrame*,
                 ScrollManager*,
                 MouseEventManager*,
                 PointerEventManager*,
                 SelectionController*);
  DECLARE_TRACE();

  void clear();

  HitTestRequest::HitTestRequestType getHitTypeForGestureType(
      PlatformEvent::EventType);
  WebInputEventResult handleGestureEventInFrame(
      const GestureEventWithHitTestResults&);

  // TODO(nzolghadr): This can probably be hidden and the related logic
  // be moved to this class (see crrev.com/112023010). Since that might cause
  // regression it's better to move that logic in another change.
  double getLastShowPressTimestamp() const;

 private:
  WebInputEventResult handleGestureShowPress();
  WebInputEventResult handleGestureTapDown(
      const GestureEventWithHitTestResults&);
  WebInputEventResult handleGestureTap(const GestureEventWithHitTestResults&);
  WebInputEventResult handleGestureLongPress(
      const GestureEventWithHitTestResults&);
  WebInputEventResult handleGestureLongTap(
      const GestureEventWithHitTestResults&);
  WebInputEventResult handleGestureTwoFingerTap(
      const GestureEventWithHitTestResults&);

  WebInputEventResult sendContextMenuEventForGesture(
      const GestureEventWithHitTestResults&);

  FrameHost* frameHost() const;

  // NOTE: If adding a new field to this class please ensure that it is
  // cleared if needed in |GestureManager::clear()|.

  const Member<LocalFrame> m_frame;

  Member<ScrollManager> m_scrollManager;
  Member<MouseEventManager> m_mouseEventManager;
  Member<PointerEventManager> m_pointerEventManager;

  // Set on GestureTapDown if the |pointerdown| event corresponding to the
  // triggering |touchstart| event was canceled. This suppresses mouse event
  // firing for the current gesture sequence (i.e. until next GestureTapDown).
  bool m_suppressMouseEventsFromGestures;

  bool m_longTapShouldInvokeContextMenu;

  const Member<SelectionController> m_selectionController;

  double m_lastShowPressTimestamp;
};

}  // namespace blink

#endif  // GestureManager_h
