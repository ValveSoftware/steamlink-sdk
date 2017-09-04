// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TopDocumentRootScrollerController_h
#define TopDocumentRootScrollerController_h

#include "core/CoreExport.h"
#include "core/page/scrolling/RootScrollerController.h"
#include "platform/heap/Handle.h"

namespace blink {

class Element;
class FrameHost;
class GraphicsLayer;
class RootFrameViewport;
class ScrollStateCallback;
class ViewportScrollCallback;

// This class manages the the page level aspects of the root scroller.  That
// is, given all the iframes on a page and their individual root scrollers,
// this class will determine which ultimate Element should be used as the root
// scroller and ensures that Element is used to scroll browser controls and
// provide overscroll effects.
// TODO(bokan): This class is currently OOPIF unaware. crbug.com/642378.
class CORE_EXPORT TopDocumentRootScrollerController
    : public GarbageCollected<TopDocumentRootScrollerController> {
 public:
  static TopDocumentRootScrollerController* create(FrameHost&);

  DECLARE_TRACE();

  // This class needs to be informed of changes to compositing so that it can
  // update the compositor when the effective root scroller changes.
  void didUpdateCompositing();

  // This method needs to be called to create a ViewportScrollCallback that
  // will be used to apply viewport scrolling actions like browser controls
  // movement and overscroll glow.
  void initializeViewportScrollCallback(RootFrameViewport&);

  // Returns true if the given ScrollStateCallback is the
  // ViewportScrollCallback managed by this class.
  // TODO(bokan): Temporarily needed to allow ScrollCustomization to
  // differentiate between real custom callback and the built-in viewport
  // apply scroll. crbug.com/623079.
  bool isViewportScrollCallback(const ScrollStateCallback*) const;

  // Returns the GraphicsLayer for the global root scroller.
  GraphicsLayer* rootScrollerLayer() const;

  // Returns the Element that's the global root scroller.
  Element* globalRootScroller() const;

  // Called when the root scroller in any frames on the page has changed.
  void didChangeRootScroller();

 private:
  TopDocumentRootScrollerController(FrameHost&);

  // Calculates the Element that should be the globalRootScroller. On a
  // simple page, this will simply the root frame's effectiveRootScroller but
  // if the root scroller is set to an iframe, this will then descend into
  // the iframe to find its effective root scroller.
  Element* findGlobalRootScrollerElement();

  // Should be called to ensure the correct element is currently set as the
  // global root scroller and that all appropriate state changes are made if
  // it changes.
  void recomputeGlobalRootScroller();

  Document* topDocument() const;

  void setNeedsCompositingInputsUpdateOnGlobalRootScroller();

  // The apply-scroll callback that moves browser controls and produces
  // overscroll effects. This class makes sure this callback is set on the
  // appropriate root scroller element.
  Member<ViewportScrollCallback> m_viewportApplyScroll;

  // The page level root scroller. i.e. The actual element for which
  // scrolling should move browser controls and produce overscroll glow. Once an
  // m_viewportApplyScroll has been created, it will always be set on this
  // Element.
  WeakMember<Element> m_globalRootScroller;

  WeakMember<FrameHost> m_frameHost;
};

}  // namespace blink

#endif  // RootScrollerController_h
