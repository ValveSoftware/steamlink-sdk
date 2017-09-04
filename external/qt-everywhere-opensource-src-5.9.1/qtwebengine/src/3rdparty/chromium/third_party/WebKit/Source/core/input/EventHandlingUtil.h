// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EventHandlingUtil_h
#define EventHandlingUtil_h

#include "core/layout/HitTestResult.h"
#include "core/page/EventWithHitTestResults.h"
#include "platform/geometry/LayoutPoint.h"
#include "public/platform/WebInputEventResult.h"

namespace blink {

class LocalFrame;
class ScrollableArea;
class PaintLayer;

namespace EventHandlingUtil {

HitTestResult hitTestResultInFrame(
    LocalFrame*,
    const LayoutPoint&,
    HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly |
                                                 HitTestRequest::Active);

WebInputEventResult mergeEventResult(WebInputEventResult resultA,
                                     WebInputEventResult resultB);
WebInputEventResult toWebInputEventResult(DispatchEventResult);

PaintLayer* layerForNode(Node*);
ScrollableArea* associatedScrollableArea(const PaintLayer*);

ContainerNode* parentForClickEvent(const Node&);

LayoutPoint contentPointFromRootFrame(LocalFrame*,
                                      const IntPoint& pointInRootFrame);

MouseEventWithHitTestResults performMouseEventHitTest(
    LocalFrame*,
    const HitTestRequest&,
    const PlatformMouseEvent&);

}  // namespace EventHandlingUtil

}  // namespace blink

#endif  // EventHandlingUtil_h
