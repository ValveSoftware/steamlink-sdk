// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RootFrameViewport_h
#define RootFrameViewport_h

#include "core/CoreExport.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

class FrameView;
class LayoutRect;

// ScrollableArea for the root frame's viewport. This class ties together the
// concepts of layout and visual viewports, used in pinch-to-zoom. This class
// takes two ScrollableAreas, one for the visual viewport and one for the
// layout viewport, and delegates and composes the ScrollableArea API as needed
// between them. For most scrolling APIs, this class will split the scroll up
// between the two viewports in accord with the pinch-zoom semantics. For other
// APIs that don't make sense on the combined viewport, the call is delegated to
// the layout viewport. Thus, we could say this class is a decorator on the
// FrameView scrollable area that adds pinch-zoom semantics to scrolling.
class CORE_EXPORT RootFrameViewport final
    : public GarbageCollectedFinalized<RootFrameViewport>,
      public ScrollableArea {
  USING_GARBAGE_COLLECTED_MIXIN(RootFrameViewport);

 public:
  static RootFrameViewport* create(ScrollableArea& visualViewport,
                                   ScrollableArea& layoutViewport) {
    return new RootFrameViewport(visualViewport, layoutViewport);
  }

  DECLARE_VIRTUAL_TRACE();

  void setLayoutViewport(ScrollableArea&);
  ScrollableArea& layoutViewport() const;

  // Convert from the root content document's coordinate space, into the
  // coordinate space of the layout viewport's content. In the normal case,
  // this will be a no-op since the root FrameView is the layout viewport and
  // so the root content is the layout viewport's content but if the page
  // sets a custom root scroller via document.rootScroller, another element
  // may be the layout viewport.
  LayoutRect rootContentsToLayoutViewportContents(FrameView& rootFrameView,
                                                  const LayoutRect&) const;

  void restoreToAnchor(const ScrollOffset&);

  // Callback whenever the visual viewport changes scroll position or scale.
  void didUpdateVisualViewport();

  // ScrollableArea Implementation
  bool isRootFrameViewport() const override { return true; }
  void setScrollOffset(const ScrollOffset&,
                       ScrollType,
                       ScrollBehavior = ScrollBehaviorInstant) override;
  LayoutRect scrollIntoView(const LayoutRect& rectInContent,
                            const ScrollAlignment& alignX,
                            const ScrollAlignment& alignY,
                            ScrollType = ProgrammaticScroll) override;
  IntRect visibleContentRect(
      IncludeScrollbarsInRect = ExcludeScrollbars) const override;
  bool shouldUseIntegerScrollOffset() const override;
  LayoutRect visualRectForScrollbarParts() const override {
    ASSERT_NOT_REACHED();
    return LayoutRect();
  }
  bool isActive() const override;
  int scrollSize(ScrollbarOrientation) const override;
  bool isScrollCornerVisible() const override;
  IntRect scrollCornerRect() const override;
  void updateScrollOffset(const ScrollOffset&, ScrollType) override;
  IntSize scrollOffsetInt() const override;
  ScrollOffset scrollOffset() const override;
  IntSize minimumScrollOffsetInt() const override;
  IntSize maximumScrollOffsetInt() const override;
  ScrollOffset maximumScrollOffset() const override;
  IntSize contentsSize() const override;
  bool scrollbarsCanBeActive() const override;
  IntRect scrollableAreaBoundingBox() const override;
  bool userInputScrollable(ScrollbarOrientation) const override;
  bool shouldPlaceVerticalScrollbarOnLeft() const override;
  void scrollControlWasSetNeedsPaintInvalidation() override;
  GraphicsLayer* layerForContainer() const override;
  GraphicsLayer* layerForScrolling() const override;
  GraphicsLayer* layerForHorizontalScrollbar() const override;
  GraphicsLayer* layerForVerticalScrollbar() const override;
  GraphicsLayer* layerForScrollCorner() const override;
  int horizontalScrollbarHeight(
      OverlayScrollbarClipBehavior = IgnoreOverlayScrollbarSize) const override;
  int verticalScrollbarWidth(
      OverlayScrollbarClipBehavior = IgnoreOverlayScrollbarSize) const override;
  ScrollResult userScroll(ScrollGranularity, const FloatSize&) override;
  bool scrollAnimatorEnabled() const override;
  HostWindow* getHostWindow() const override;
  void serviceScrollAnimations(double) override;
  void updateCompositorScrollAnimations() override;
  void cancelProgrammaticScrollAnimation() override;
  ScrollBehavior scrollBehaviorStyle() const override;
  Widget* getWidget() override;
  void clearScrollableArea() override;
  LayoutBox* layoutBox() const override;
  FloatQuad localToVisibleContentQuad(const FloatQuad&,
                                      const LayoutObject*,
                                      unsigned = 0) const final;

 private:
  RootFrameViewport(ScrollableArea& visualViewport,
                    ScrollableArea& layoutViewport);

  enum ViewportToScrollFirst { VisualViewport, LayoutViewport };

  ScrollOffset scrollOffsetFromScrollAnimators() const;

  void distributeScrollBetweenViewports(const ScrollOffset&,
                                        ScrollType,
                                        ScrollBehavior,
                                        ViewportToScrollFirst);

  // If either of the layout or visual viewports are scrolled explicitly (i.e.
  // not through this class), their updated offset will not be reflected in this
  // class' animator so use this method to pull updated values when necessary.
  void updateScrollAnimator();

  ScrollableArea& visualViewport() const {
    ASSERT(m_visualViewport);
    return *m_visualViewport;
  }

  Member<ScrollableArea> m_visualViewport;
  Member<ScrollableArea> m_layoutViewport;
};

DEFINE_TYPE_CASTS(RootFrameViewport,
                  ScrollableArea,
                  scrollableArea,
                  scrollableArea->isRootFrameViewport(),
                  scrollableArea.isRootFrameViewport());

}  // namespace blink

#endif  // RootFrameViewport_h
