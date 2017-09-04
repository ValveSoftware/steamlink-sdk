// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/EventHandlingUtil.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/paint/PaintLayer.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {
namespace EventHandlingUtil {

HitTestResult hitTestResultInFrame(LocalFrame* frame,
                                   const LayoutPoint& point,
                                   HitTestRequest::HitTestRequestType hitType) {
  HitTestResult result(HitTestRequest(hitType), point);

  if (!frame || frame->contentLayoutItem().isNull())
    return result;
  if (frame->view()) {
    IntRect rect = frame->view()->visibleContentRect(IncludeScrollbars);
    if (!rect.contains(roundedIntPoint(point)))
      return result;
  }
  frame->contentLayoutItem().hitTest(result);
  return result;
}

WebInputEventResult mergeEventResult(WebInputEventResult resultA,
                                     WebInputEventResult resultB) {
  // The ordering of the enumeration is specific. There are times that
  // multiple events fire and we need to combine them into a single
  // result code. The enumeration is based on the level of consumption that
  // is most significant. The enumeration is ordered with smaller specified
  // numbers first. Examples of merged results are:
  // (HandledApplication, HandledSystem) -> HandledSystem
  // (NotHandled, HandledApplication) -> HandledApplication
  static_assert(static_cast<int>(WebInputEventResult::NotHandled) == 0,
                "WebInputEventResult not ordered");
  static_assert(static_cast<int>(WebInputEventResult::HandledSuppressed) <
                    static_cast<int>(WebInputEventResult::HandledApplication),
                "WebInputEventResult not ordered");
  static_assert(static_cast<int>(WebInputEventResult::HandledApplication) <
                    static_cast<int>(WebInputEventResult::HandledSystem),
                "WebInputEventResult not ordered");
  return static_cast<WebInputEventResult>(
      max(static_cast<int>(resultA), static_cast<int>(resultB)));
}

WebInputEventResult toWebInputEventResult(DispatchEventResult result) {
  switch (result) {
    case DispatchEventResult::NotCanceled:
      return WebInputEventResult::NotHandled;
    case DispatchEventResult::CanceledByEventHandler:
      return WebInputEventResult::HandledApplication;
    case DispatchEventResult::CanceledByDefaultEventHandler:
      return WebInputEventResult::HandledSystem;
    case DispatchEventResult::CanceledBeforeDispatch:
      return WebInputEventResult::HandledSuppressed;
    default:
      NOTREACHED();
      return WebInputEventResult::HandledSystem;
  }
}

PaintLayer* layerForNode(Node* node) {
  if (!node)
    return nullptr;

  LayoutObject* layoutObject = node->layoutObject();
  if (!layoutObject)
    return nullptr;

  PaintLayer* layer = layoutObject->enclosingLayer();
  if (!layer)
    return nullptr;

  return layer;
}

ScrollableArea* associatedScrollableArea(const PaintLayer* layer) {
  if (PaintLayerScrollableArea* scrollableArea = layer->getScrollableArea()) {
    if (scrollableArea->scrollsOverflow())
      return scrollableArea;
  }

  return nullptr;
}

ContainerNode* parentForClickEvent(const Node& node) {
  // IE doesn't dispatch click events for mousedown/mouseup events across form
  // controls.
  if (node.isHTMLElement() && toHTMLElement(node).isInteractiveContent())
    return nullptr;

  return FlatTreeTraversal::parent(node);
}

LayoutPoint contentPointFromRootFrame(LocalFrame* frame,
                                      const IntPoint& pointInRootFrame) {
  FrameView* view = frame->view();
  // FIXME: Is it really OK to use the wrong coordinates here when view is 0?
  // Historically the code would just crash; this is clearly no worse than that.
  return view ? view->rootFrameToContents(pointInRootFrame) : pointInRootFrame;
}

MouseEventWithHitTestResults performMouseEventHitTest(
    LocalFrame* frame,
    const HitTestRequest& request,
    const PlatformMouseEvent& mev) {
  DCHECK(frame);
  DCHECK(frame->document());

  return frame->document()->performMouseEventHitTest(
      request, contentPointFromRootFrame(frame, mev.position()), mev);
}

}  // namespace EventHandlingUtil
}  // namespace blink
