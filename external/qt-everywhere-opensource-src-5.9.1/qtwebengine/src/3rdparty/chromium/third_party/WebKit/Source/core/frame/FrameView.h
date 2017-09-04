/*
   Copyright (C) 1997 Martin Jones (mjones@kde.org)
             (C) 1998 Waldo Bastian (bastian@kde.org)
             (C) 1998, 1999 Torben Weis (weis@kde.org)
             (C) 1999 Lars Knoll (knoll@kde.org)
             (C) 1999 Antti Koivisto (koivisto@kde.org)
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
   reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef FrameView_h
#define FrameView_h

#include "core/CoreExport.h"
#include "core/dom/DocumentLifecycle.h"
#include "core/frame/FrameViewAutoSizeInfo.h"
#include "core/frame/LayoutSubtreeRootList.h"
#include "core/frame/RootFrameViewport.h"
#include "core/layout/ScrollAnchor.h"
#include "core/paint/FirstMeaningfulPaintDetector.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintInvalidationCapableScrollableArea.h"
#include "core/paint/PaintPhase.h"
#include "core/paint/ScrollbarManager.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/Widget.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/scroll/Scrollbar.h"
#include "public/platform/WebDisplayMode.h"
#include "public/platform/WebRect.h"
#include "wtf/Allocator.h"
#include "wtf/AutoReset.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/ListHashSet.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class AXObjectCache;
class ComputedStyle;
class DocumentLifecycle;
class Cursor;
class Element;
class ElementVisibilityObserver;
class FloatSize;
class JSONArray;
class JSONObject;
class LayoutItem;
class LayoutViewItem;
class LayoutPart;
class LocalFrame;
class KURL;
class Node;
class LayoutAnalyzer;
class LayoutBox;
class LayoutEmbeddedObject;
class LayoutObject;
class LayoutReplaced;
class LayoutScrollbarPart;
class LayoutView;
class PaintArtifactCompositor;
class PaintController;
class PaintInvalidationState;
class Page;
class ScrollingCoordinator;
class TracedValue;
struct AnnotatedRegionValue;
struct CompositedSelection;

typedef unsigned long long DOMTimeStamp;

class CORE_EXPORT FrameView final
    : public Widget,
      public PaintInvalidationCapableScrollableArea {
  USING_GARBAGE_COLLECTED_MIXIN(FrameView);

  friend class PaintControllerPaintTestBase;
  friend class Internals;
  friend class LayoutPart;  // for invalidateTreeIfNeeded

 public:
  static FrameView* create(LocalFrame&);
  static FrameView* create(LocalFrame&, const IntSize& initialSize);

  ~FrameView() override;

  void invalidateRect(const IntRect&) override;
  void setFrameRect(const IntRect&) override;

  LocalFrame& frame() const {
    ASSERT(m_frame);
    return *m_frame;
  }

  Page* page() const;

  // TODO(pilgrim) replace all instances of layoutView() with layoutViewItem()
  // https://crbug.com/499321
  LayoutView* layoutView() const;
  LayoutViewItem layoutViewItem() const;

  void setCanHaveScrollbars(bool);

  Scrollbar* createScrollbar(ScrollbarOrientation);

  void setContentsSize(const IntSize&);

  void layout();
  bool didFirstLayout() const;
  void scheduleRelayout();
  void scheduleRelayoutOfSubtree(LayoutObject*);
  bool layoutPending() const;
  bool isInPerformLayout() const;

  void clearLayoutSubtreeRoot(const LayoutObject&);
  void addOrthogonalWritingModeRoot(LayoutBox&);
  void removeOrthogonalWritingModeRoot(LayoutBox&);
  bool hasOrthogonalWritingModeRoots() const;
  void layoutOrthogonalWritingModeRoots();
  int layoutCount() const { return m_layoutCount; }

  void countObjectsNeedingLayout(unsigned& needsLayoutObjects,
                                 unsigned& totalObjects,
                                 bool& isPartial);

  bool needsLayout() const;
  bool checkDoesNotNeedLayout() const;
  void setNeedsLayout();

  void setNeedsUpdateWidgetGeometries() {
    m_needsUpdateWidgetGeometries = true;
  }

  // Methods for getting/setting the size Blink should use to layout the
  // contents.
  // NOTE: Scrollbar exclusion is based on the FrameView's scrollbars. To
  // exclude scrollbars on the root PaintLayer, use LayoutView::layoutSize.
  IntSize layoutSize(IncludeScrollbarsInRect = ExcludeScrollbars) const;
  void setLayoutSize(const IntSize&);

  // If this is set to false, the layout size will need to be explicitly set by
  // the owner.  E.g. WebViewImpl sets its mainFrame's layout size manually
  void setLayoutSizeFixedToFrameSize(bool isFixed) {
    m_layoutSizeFixedToFrameSize = isFixed;
  }
  bool layoutSizeFixedToFrameSize() { return m_layoutSizeFixedToFrameSize; }

  void setInitialViewportSize(const IntSize&);
  int initialViewportWidth() const;
  int initialViewportHeight() const;

  void updateAcceleratedCompositingSettings();

  void recalcOverflowAfterStyleChange();

  bool isEnclosedInCompositingLayer() const;

  void dispose() override;
  void detachScrollbars();
  void recalculateCustomScrollbarStyle();
  void invalidateAllCustomScrollbarsOnActiveChanged();

  void clear();

  bool isTransparent() const;
  void setTransparent(bool isTransparent);

  // True if the FrameView is not transparent, and the base background color is
  // opaque.
  bool hasOpaqueBackground() const;

  Color baseBackgroundColor() const;
  void setBaseBackgroundColor(const Color&);
  void updateBackgroundRecursively(const Color&, bool);

  void adjustViewSize();
  void adjustViewSizeAndLayout();

  // Scale used to convert incoming input events.
  float inputEventsScaleFactor() const;

  // Offset used to convert incoming input events while emulating device metics.
  IntSize inputEventsOffsetForEmulation() const;
  void setInputEventsTransformForEmulation(const IntSize&, float);

  void didChangeScrollOffset();
  void didUpdateElasticOverscroll();

  void viewportSizeChanged(bool widthChanged, bool heightChanged);

  AtomicString mediaType() const;
  void setMediaType(const AtomicString&);
  void adjustMediaTypeForPrinting(bool printing);

  WebDisplayMode displayMode() { return m_displayMode; }
  void setDisplayMode(WebDisplayMode);

  // Fixed-position objects.
  typedef HashSet<LayoutObject*> ViewportConstrainedObjectSet;
  void addViewportConstrainedObject(LayoutObject*);
  void removeViewportConstrainedObject(LayoutObject*);
  const ViewportConstrainedObjectSet* viewportConstrainedObjects() const {
    return m_viewportConstrainedObjects.get();
  }
  bool hasViewportConstrainedObjects() const {
    return m_viewportConstrainedObjects &&
           m_viewportConstrainedObjects->size() > 0;
  }

  // Objects with background-attachment:fixed.
  void addBackgroundAttachmentFixedObject(LayoutObject*);
  void removeBackgroundAttachmentFixedObject(LayoutObject*);
  bool hasBackgroundAttachmentFixedObjects() const {
    return m_backgroundAttachmentFixedObjects.size();
  }
  void invalidateBackgroundAttachmentFixedObjects();

  void handleLoadCompleted();

  void updateDocumentAnnotatedRegions() const;

  void didAttachDocument();

  void restoreScrollbar();

  void postLayoutTimerFired(TimerBase*);

  bool safeToPropagateScrollToParent() const {
    return m_safeToPropagateScrollToParent;
  }
  void setSafeToPropagateScrollToParent(bool isSafe) {
    m_safeToPropagateScrollToParent = isSafe;
  }

  void addPart(LayoutPart*);
  void removePart(LayoutPart*);

  void updateWidgetGeometries();

  void addPartToUpdate(LayoutEmbeddedObject&);

  Color documentBackgroundColor() const;

  // Run all needed lifecycle stages. After calling this method, all frames will
  // be in the lifecycle state PaintInvalidationClean.  If lifecycle throttling
  // is allowed (see DocumentLifecycle::AllowThrottlingScope), some frames may
  // skip the lifecycle update (e.g., based on visibility) and will not end up
  // being PaintInvalidationClean.
  void updateAllLifecyclePhases();

  // Everything except paint (the last phase).
  void updateAllLifecyclePhasesExceptPaint();

  // Computes the style, layout and compositing lifecycle stages if needed.
  // After calling this method, all frames will be in a lifecycle
  // state >= CompositingClean, and scrolling has been updated (unless
  // throttling is allowed).
  void updateLifecycleToCompositingCleanPlusScrolling();

  // Computes only the style and layout lifecycle stages.
  // After calling this method, all frames will be in a lifecycle
  // state >= LayoutClean (unless throttling is allowed).
  void updateLifecycleToLayoutClean();

  void scheduleVisualUpdateForPaintInvalidationIfNeeded();

  bool invalidateViewportConstrainedObjects();

  void incrementLayoutObjectCount() { m_layoutObjectCounter.increment(); }
  void incrementVisuallyNonEmptyCharacterCount(unsigned);
  void incrementVisuallyNonEmptyPixelCount(const IntSize&);
  bool isVisuallyNonEmpty() const { return m_isVisuallyNonEmpty; }
  void setIsVisuallyNonEmpty() { m_isVisuallyNonEmpty = true; }
  void enableAutoSizeMode(const IntSize& minSize, const IntSize& maxSize);
  void disableAutoSizeMode();

  void forceLayoutForPagination(const FloatSize& pageSize,
                                const FloatSize& originalPageSize,
                                float maximumShrinkFactor);

  enum UrlFragmentBehavior { UrlFragmentScroll, UrlFragmentDontScroll };
  // Updates the fragment anchor element based on URL's fragment identifier.
  // Updates corresponding ':target' CSS pseudo class on the anchor element.
  // If |UrlFragmentScroll| is passed in then makes the anchor element
  // focused and also visible by scrolling to it. The scroll offset is
  // maintained during the frame loading process.
  void processUrlFragment(const KURL&, UrlFragmentBehavior = UrlFragmentScroll);
  void clearFragmentAnchor();

  // Methods to convert points and rects between the coordinate space of the
  // layoutItem, and this view.
  IntRect convertFromLayoutItem(const LayoutItem&, const IntRect&) const;
  IntRect convertToLayoutItem(const LayoutItem&, const IntRect&) const;
  IntPoint convertFromLayoutItem(const LayoutItem&, const IntPoint&) const;
  IntPoint convertToLayoutItem(const LayoutItem&, const IntPoint&) const;

  bool isFrameViewScrollCorner(LayoutScrollbarPart* scrollCorner) const {
    return m_scrollCorner == scrollCorner;
  }

  enum ScrollingReasons {
    Scrollable,
    NotScrollableNoOverflow,
    NotScrollableNotVisible,
    NotScrollableExplicitlyDisabled
  };

  ScrollingReasons getScrollingReasons();
  bool isScrollable() override;
  bool isProgrammaticallyScrollable() override;

  enum ScrollbarModesCalculationStrategy { RulesFromWebContentOnly, AnyRule };
  void calculateScrollbarModes(ScrollbarMode& hMode,
                               ScrollbarMode& vMode,
                               ScrollbarModesCalculationStrategy = AnyRule);

  IntPoint lastKnownMousePosition() const override;
  bool shouldSetCursor() const;

  void setCursor(const Cursor&);

  bool scrollbarsCanBeActive() const override;
  void scrollbarVisibilityChanged() override;

  // FIXME: Remove this method once plugin loading is decoupled from layout.
  void flushAnyPendingPostLayoutTasks();

  bool shouldSuspendScrollAnimations() const override;
  void scrollbarStyleChanged() override;

  LayoutReplaced* embeddedReplacedContent() const;

  static void setInitialTracksPaintInvalidationsForTesting(bool);

  // These methods are for testing.
  void setTracksPaintInvalidations(bool);
  bool isTrackingPaintInvalidations() const {
    return m_trackedObjectPaintInvalidations.get();
  }
  void trackObjectPaintInvalidation(const DisplayItemClient&,
                                    PaintInvalidationReason);
  std::unique_ptr<JSONArray> trackedObjectPaintInvalidationsAsJSON() const;

  using ScrollableAreaSet = HeapHashSet<Member<ScrollableArea>>;
  void addScrollableArea(ScrollableArea*);
  void removeScrollableArea(ScrollableArea*);
  const ScrollableAreaSet* scrollableAreas() const {
    return m_scrollableAreas.get();
  }

  void addAnimatingScrollableArea(ScrollableArea*);
  void removeAnimatingScrollableArea(ScrollableArea*);
  const ScrollableAreaSet* animatingScrollableAreas() const {
    return m_animatingScrollableAreas.get();
  }

  // With CSS style "resize:" enabled, a little resizer handle will appear at
  // the bottom right of the object. We keep track of these resizer areas for
  // checking if touches (implemented using Scroll gesture) are targeting the
  // resizer.
  typedef HashSet<LayoutBox*> ResizerAreaSet;
  void addResizerArea(LayoutBox&);
  void removeResizerArea(LayoutBox&);
  const ResizerAreaSet* resizerAreas() const { return m_resizerAreas.get(); }

  bool shouldUseIntegerScrollOffset() const override;

  bool isActive() const override;

  // Override scrollbar notifications to update the AXObject cache.
  void didAddScrollbar(Scrollbar&, ScrollbarOrientation) override;

  // FIXME: This should probably be renamed as the 'inSubtreeLayout' parameter
  // passed around the FrameView layout methods can be true while this returns
  // false.
  bool isSubtreeLayout() const { return !m_layoutSubtreeRootList.isEmpty(); }

  // Sets the tickmarks for the FrameView, overriding the default behavior
  // which is to display the tickmarks corresponding to find results.
  // If |m_tickmarks| is empty, the default behavior is restored.
  void setTickmarks(const Vector<IntRect>& tickmarks) {
    m_tickmarks = tickmarks;
    invalidatePaintForTickmarks();
  }

  void invalidatePaintForTickmarks();

  // Since the compositor can resize the viewport due to browser controls and
  // commit scroll offsets before a WebView::resize occurs, we need to adjust
  // our scroll extents to prevent clamping the scroll offsets.
  void setBrowserControlsViewportAdjustment(float);
  IntSize browserControlsSize() const {
    return IntSize(0, ceilf(m_browserControlsViewportAdjustment));
  }

  IntSize maximumScrollOffsetInt() const override;

  // ScrollableArea interface
  void getTickmarks(Vector<IntRect>&) const override;
  IntRect scrollableAreaBoundingBox() const override;
  bool scrollAnimatorEnabled() const override;
  bool usesCompositedScrolling() const override;
  bool shouldScrollOnMainThread() const override;
  GraphicsLayer* layerForScrolling() const override;
  GraphicsLayer* layerForHorizontalScrollbar() const override;
  GraphicsLayer* layerForVerticalScrollbar() const override;
  GraphicsLayer* layerForScrollCorner() const override;
  int scrollSize(ScrollbarOrientation) const override;
  bool isScrollCornerVisible() const override;
  bool userInputScrollable(ScrollbarOrientation) const override;
  bool shouldPlaceVerticalScrollbarOnLeft() const override;
  Widget* getWidget() override;
  CompositorAnimationTimeline* compositorAnimationTimeline() const override;
  LayoutBox* layoutBox() const override;
  FloatQuad localToVisibleContentQuad(const FloatQuad&,
                                      const LayoutObject*,
                                      unsigned = 0) const final;

  LayoutRect scrollIntoView(const LayoutRect& rectInContent,
                            const ScrollAlignment& alignX,
                            const ScrollAlignment& alignY,
                            ScrollType = ProgrammaticScroll) override;

  // The window that hosts the FrameView. The FrameView will communicate scrolls
  // and repaints to the host window in the window's coordinate space.
  HostWindow* getHostWindow() const;

  typedef HeapHashSet<Member<Widget>> ChildrenWidgetSet;

  // Functions for child manipulation and inspection.
  void setParent(Widget*) override;
  void removeChild(Widget*);
  void addChild(Widget*);
  const ChildrenWidgetSet* children() const { return &m_children; }

  // If the scroll view does not use a native widget, then it will have
  // cross-platform Scrollbars. These functions can be used to obtain those
  // scrollbars.
  Scrollbar* horizontalScrollbar() const override {
    return m_scrollbarManager.horizontalScrollbar();
  }
  Scrollbar* verticalScrollbar() const override {
    return m_scrollbarManager.verticalScrollbar();
  }
  LayoutScrollbarPart* scrollCorner() const override { return m_scrollCorner; }

  void positionScrollbarLayers();

  // Functions for setting and retrieving the scrolling mode in each axis
  // (horizontal/vertical). The mode has values of AlwaysOff, AlwaysOn, and
  // Auto. AlwaysOff means never show a scrollbar, AlwaysOn means always show a
  // scrollbar.  Auto means show a scrollbar only when one is needed.
  // Note that for platforms with native widgets, these modes are considered
  // advisory. In other words the underlying native widget may choose not to
  // honor the requested modes.
  void setScrollbarModes(ScrollbarMode horizontalMode,
                         ScrollbarMode verticalMode,
                         bool horizontalLock = false,
                         bool verticalLock = false);
  void setHorizontalScrollbarMode(ScrollbarMode mode, bool lock = false) {
    setScrollbarModes(mode, verticalScrollbarMode(), lock,
                      verticalScrollbarLock());
  }
  void setVerticalScrollbarMode(ScrollbarMode mode, bool lock = false) {
    setScrollbarModes(horizontalScrollbarMode(), mode,
                      horizontalScrollbarLock(), lock);
  }
  ScrollbarMode horizontalScrollbarMode() const {
    return m_horizontalScrollbarMode;
  }
  ScrollbarMode verticalScrollbarMode() const {
    return m_verticalScrollbarMode;
  }

  void setHorizontalScrollbarLock(bool lock = true) {
    m_horizontalScrollbarLock = lock;
  }
  bool horizontalScrollbarLock() const { return m_horizontalScrollbarLock; }
  void setVerticalScrollbarLock(bool lock = true) {
    m_verticalScrollbarLock = lock;
  }
  bool verticalScrollbarLock() const { return m_verticalScrollbarLock; }

  void setScrollingModesLock(bool lock = true) {
    m_horizontalScrollbarLock = m_verticalScrollbarLock = lock;
  }

  bool canHaveScrollbars() const {
    return horizontalScrollbarMode() != ScrollbarAlwaysOff ||
           verticalScrollbarMode() != ScrollbarAlwaysOff;
  }

  // The visible content rect has a location that is the scrolled offset of
  // the document. The width and height are the layout viewport width and
  // height. By default the scrollbars themselves are excluded from this
  // rectangle, but an optional boolean argument allows them to be included.
  IntRect visibleContentRect(
      IncludeScrollbarsInRect = ExcludeScrollbars) const override;
  IntSize visibleContentSize(IncludeScrollbarsInRect = ExcludeScrollbars) const;

  // Clips the provided rect to the visible content area. For this purpose, we
  // also query the chrome client for any active overrides to the visible area
  // (e.g. DevTool's viewport override).
  void clipPaintRect(FloatRect*) const;

  // Functions for getting/setting the size of the document contained inside the
  // FrameView (as an IntSize or as individual width and height values).
  // Always at least as big as the visibleWidth()/visibleHeight().
  IntSize contentsSize() const override;
  int contentsWidth() const { return contentsSize().width(); }
  int contentsHeight() const { return contentsSize().height(); }

  // Functions for querying the current scrolled offset (both as a point, a
  // size, or as individual X and Y values).  Be careful in using the Float
  // version scrollOffset() and scrollOffset(). They are meant to be used to
  // communicate the fractional scroll offset with chromium compositor which can
  // do sub-pixel positioning.  Do not call these if the scroll offset is used
  // in Blink for positioning. Use the Int version instead.
  IntSize scrollOffsetInt() const override {
    return toIntSize(visibleContentRect().location());
  }
  ScrollOffset scrollOffset() const override { return m_scrollOffset; }
  ScrollOffset pendingScrollDelta() const { return m_pendingScrollDelta; }
  IntSize minimumScrollOffsetInt()
      const override;  // The minimum offset we can be scrolled to.
  int scrollX() const { return scrollOffsetInt().width(); }
  int scrollY() const { return scrollOffsetInt().height(); }

  // Scroll the actual contents of the view (either blitting or invalidating as
  // needed).
  void scrollContents(const IntSize& scrollDelta);

  // This gives us a means of blocking updating our scrollbars until the first
  // layout has occurred.
  void setScrollbarsSuppressed(bool suppressed) {
    m_scrollbarsSuppressed = suppressed;
  }
  bool scrollbarsSuppressed() const { return m_scrollbarsSuppressed; }

  // Methods for converting between this frame and other coordinate spaces.
  // For definitions and an explanation of the varous spaces, please see:
  // http://www.chromium.org/developers/design-documents/blink-coordinate-spaces
  IntPoint rootFrameToContents(const IntPoint&) const;
  FloatPoint rootFrameToContents(const FloatPoint&) const;
  IntRect rootFrameToContents(const IntRect&) const;
  IntPoint contentsToRootFrame(const IntPoint&) const;
  IntRect contentsToRootFrame(const IntRect&) const;

  IntRect viewportToContents(const IntRect&) const;
  IntRect contentsToViewport(const IntRect&) const;
  IntPoint contentsToViewport(const IntPoint&) const;
  IntPoint viewportToContents(const IntPoint&) const;

  // FIXME: Some external callers expect to get back a rect that's positioned
  // in viewport space, but sized in CSS pixels. This is an artifact of the
  // old pinch-zoom path. These callers should be converted to expect a rect
  // fully in viewport space. crbug.com/459591.
  IntRect soonToBeRemovedContentsToUnscaledViewport(const IntRect&) const;
  IntPoint soonToBeRemovedUnscaledViewportToContents(const IntPoint&) const;

  // Methods for converting between Frame and Content (i.e. Document)
  // coordinates.  Frame coordinates are relative to the top left corner of the
  // frame and so they are affected by scroll offset. Content coordinates are
  // relative to the document's top left corner and thus are not affected by
  // scroll offset.
  IntPoint contentsToFrame(const IntPoint&) const;
  IntRect contentsToFrame(const IntRect&) const;
  IntPoint frameToContents(const IntPoint&) const;
  FloatPoint frameToContents(const FloatPoint&) const;
  IntRect frameToContents(const IntRect&) const;

  // Functions for converting to screen coordinates.
  IntRect contentsToScreen(const IntRect&) const;

  // For platforms that need to hit test scrollbars from within the engine's
  // event handlers (like Win32).
  Scrollbar* scrollbarAtFramePoint(const IntPoint&);

  IntPoint convertChildToSelf(const Widget* child,
                              const IntPoint& point) const override {
    IntPoint newPoint = point;
    if (!isFrameViewScrollbar(child))
      newPoint = contentsToFrame(point);
    newPoint.moveBy(child->location());
    return newPoint;
  }

  IntPoint convertSelfToChild(const Widget* child,
                              const IntPoint& point) const override {
    IntPoint newPoint = point;
    if (!isFrameViewScrollbar(child))
      newPoint = frameToContents(point);
    newPoint.moveBy(-child->location());
    return newPoint;
  }

  // Widget override. Handles painting of the contents of the view as well as
  // the scrollbars.
  void paint(GraphicsContext&, const CullRect&) const override;
  void paint(GraphicsContext&, const GlobalPaintFlags, const CullRect&) const;
  void paintContents(GraphicsContext&,
                     const GlobalPaintFlags,
                     const IntRect& damageRect) const;

  // Widget overrides to ensure that our children's visibility status is kept up
  // to date when we get shown and hidden.
  void show() override;
  void hide() override;
  void setParentVisible(bool) override;

  bool isPointInScrollbarCorner(const IntPoint&);
  bool scrollbarCornerPresent() const;
  IntRect scrollCornerRect() const override;

  IntRect convertFromScrollbarToContainingWidget(const Scrollbar&,
                                                 const IntRect&) const override;
  IntRect convertFromContainingWidgetToScrollbar(const Scrollbar&,
                                                 const IntRect&) const override;
  IntPoint convertFromScrollbarToContainingWidget(
      const Scrollbar&,
      const IntPoint&) const override;
  IntPoint convertFromContainingWidgetToScrollbar(
      const Scrollbar&,
      const IntPoint&) const override;

  bool isFrameView() const override { return true; }

  DECLARE_VIRTUAL_TRACE();
  void notifyPageThatContentAreaWillPaint() const;
  FrameView* parentFrameView() const;

  // Returns the scrollable area for the frame. For the root frame, this will
  // be the RootFrameViewport, which adds pinch-zoom semantics to scrolling.
  // For non-root frames, this will be the the ScrollableArea used by the
  // FrameView, depending on whether root-layer-scrolls is enabled.
  ScrollableArea* getScrollableArea();

  // Used to get at the underlying layoutViewport in the rare instances where
  // we actually want to scroll *just* the layout viewport (e.g. when sending
  // deltas from CC). For typical scrolling cases, use getScrollableArea().
  ScrollableArea* layoutViewportScrollableArea();

  // If this is the main frame, this will return the RootFrameViewport used
  // to scroll the main frame. Otherwise returns nullptr. Unless you need a
  // unique method on RootFrameViewport, you should probably use
  // getScrollableArea.
  RootFrameViewport* getRootFrameViewport();

  int viewportWidth() const;

  LayoutAnalyzer* layoutAnalyzer() { return m_analyzer.get(); }

  // Returns true if this frame should not render or schedule visual updates.
  bool shouldThrottleRendering() const;

  // Returns true if this frame could potentially skip rendering and avoid
  // scheduling visual updates.
  bool canThrottleRendering() const;
  bool isHiddenForThrottling() const { return m_hiddenForThrottling; }
  void setupRenderThrottling();

  // For testing, run pending intersection observer notifications for this
  // frame.
  void updateRenderThrottlingStatusForTesting();

  void beginLifecycleUpdates();

  // Paint properties for SPv2 Only.
  void setPreTranslation(
      PassRefPtr<TransformPaintPropertyNode> preTranslation) {
    m_preTranslation = preTranslation;
  }
  TransformPaintPropertyNode* preTranslation() const {
    return m_preTranslation.get();
  }

  void setScrollTranslation(
      PassRefPtr<TransformPaintPropertyNode> scrollTranslation) {
    m_scrollTranslation = scrollTranslation;
  }
  TransformPaintPropertyNode* scrollTranslation() const {
    return m_scrollTranslation.get();
  }

  void setScroll(PassRefPtr<ScrollPaintPropertyNode> scroll) {
    m_scroll = scroll;
  }
  ScrollPaintPropertyNode* scroll() const { return m_scroll.get(); }

  void setContentClip(PassRefPtr<ClipPaintPropertyNode> contentClip) {
    m_contentClip = contentClip;
  }
  ClipPaintPropertyNode* contentClip() const { return m_contentClip.get(); }

  // The property tree state that should be used for painting contents. These
  // properties are either created by this FrameView or are inherited from
  // an ancestor.
  void setTotalPropertyTreeStateForContents(
      std::unique_ptr<PropertyTreeState> state) {
    m_totalPropertyTreeStateForContents = std::move(state);
  }
  const PropertyTreeState* totalPropertyTreeStateForContents() const {
    return m_totalPropertyTreeStateForContents.get();
  }

  // Paint properties (e.g., m_preTranslation, etc.) are built from the
  // FrameView's state (e.g., x(), y(), etc.) as well as inherited context.
  // When these inputs change, setNeedsPaintPropertyUpdate will cause a property
  // tree update during the next document lifecycle update.
  // TODO(pdr): Add additional granularity such as the ability to signal that
  // only a local paint property update is needed.
  void setNeedsPaintPropertyUpdate() { m_needsPaintPropertyUpdate = true; }
  void clearNeedsPaintPropertyUpdate() {
    DCHECK_EQ(lifecycle().state(), DocumentLifecycle::InPrePaint);
    m_needsPaintPropertyUpdate = false;
  }
  bool needsPaintPropertyUpdate() const { return m_needsPaintPropertyUpdate; }
  // TODO(ojan): Merge this with IntersectionObserver once it lands.
  IntRect computeVisibleArea();

  // Viewport size that should be used for viewport units (i.e. 'vh'/'vw').
  FloatSize viewportSizeForViewportUnits() const;

  ScrollAnchor* scrollAnchor() override { return &m_scrollAnchor; }
  void clearScrollAnchor();
  bool shouldPerformScrollAnchoring() const override;
  void enqueueScrollAnchoringAdjustment(ScrollableArea*);
  void performScrollAnchoringAdjustments();

  // For PaintInvalidator temporarily. TODO(wangxianzhu): Move into
  // PaintInvalidator.
  void invalidatePaintIfNeeded(const PaintInvalidationState&);

  // Only for SPv2.
  std::unique_ptr<JSONObject> compositedLayersAsJSON(LayerTreeFlags);

 protected:
  // Scroll the content via the compositor.
  bool scrollContentsFastPath(const IntSize& scrollDelta);

  // Scroll the content by invalidating everything.
  void scrollContentsSlowPath();

  ScrollBehavior scrollBehaviorStyle() const override;

  void scrollContentsIfNeeded();

  enum ComputeScrollbarExistenceOption { FirstPass, Incremental };
  void computeScrollbarExistence(bool& newHasHorizontalScrollbar,
                                 bool& newHasVerticalScrollbar,
                                 const IntSize& docSize,
                                 ComputeScrollbarExistenceOption = FirstPass);
  void updateScrollbarGeometry();

  // Called to update the scrollbars to accurately reflect the state of the
  // view.
  void updateScrollbars();
  void updateScrollbarsIfNeeded();

  class InUpdateScrollbarsScope {
    STACK_ALLOCATED();

   public:
    explicit InUpdateScrollbarsScope(FrameView* view)
        : m_scope(&view->m_inUpdateScrollbars, true) {}

   private:
    AutoReset<bool> m_scope;
  };

  // Only for LayoutPart to traverse into sub frames during paint invalidation.
  void invalidateTreeIfNeeded(const PaintInvalidationState&);

 private:
  explicit FrameView(LocalFrame&);
  class ScrollbarManager : public blink::ScrollbarManager {
    DISALLOW_NEW();

    // Helper class to manage the life cycle of Scrollbar objects.
   public:
    ScrollbarManager(FrameView& scroller) : blink::ScrollbarManager(scroller) {}

    void setHasHorizontalScrollbar(bool hasScrollbar) override;
    void setHasVerticalScrollbar(bool hasScrollbar) override;

    // TODO(ymalik): This should be hidden and all calls should go through
    // setHas*Scrollbar functions above.
    Scrollbar* createScrollbar(ScrollbarOrientation) override;

   protected:
    void destroyScrollbar(ScrollbarOrientation) override;
  };

  void updateScrollOffset(const ScrollOffset&, ScrollType) override;

  void updateLifecyclePhasesInternal(
      DocumentLifecycle::LifecycleState targetState);

  void invalidateTreeIfNeededRecursive();
  void scrollContentsIfNeededRecursive();
  void updateStyleAndLayoutIfNeededRecursive();
  void updatePaintProperties();
  void synchronizedPaint();
  void synchronizedPaintRecursively(GraphicsLayer*);

  void updateStyleAndLayoutIfNeededRecursiveInternal();
  void invalidateTreeIfNeededRecursiveInternal();

  void pushPaintArtifactToCompositor();

  void reset();
  void init();

  void clearLayoutSubtreeRootsAndMarkContainingBlocks();

  // Called when our frame rect changes (or the rect/scroll offset of an
  // ancestor changes).
  void frameRectsChanged() override;

  bool contentsInCompositedLayer() const;

  void calculateScrollbarModesFromOverflowStyle(const ComputedStyle*,
                                                ScrollbarMode& hMode,
                                                ScrollbarMode& vMode);

  void updateCounters();
  void forceLayoutParentViewIfNeeded();
  void performPreLayoutTasks();
  void performLayout(bool inSubtreeLayout);
  void scheduleOrPerformPostLayoutTasks();
  void performPostLayoutTasks();

  void maybeRecordLoadReason();

  DocumentLifecycle& lifecycle() const;

  void contentsResized() override;
  void scrollbarExistenceDidChange();

  // Override Widget methods to do point conversion via layoutObjects, in order
  // to take transforms into account.
  IntRect convertToContainingWidget(const IntRect&) const override;
  IntRect convertFromContainingWidget(const IntRect&) const override;
  IntPoint convertToContainingWidget(const IntPoint&) const override;
  IntPoint convertFromContainingWidget(const IntPoint&) const override;

  void didChangeGlobalRootScroller() override;

  void updateWidgetGeometriesIfNeeded();

  bool wasViewportResized();
  void sendResizeEventIfNeeded();

  void updateParentScrollableAreaSet();

  void scheduleUpdateWidgetsIfNecessary();
  void updateWidgetsTimerFired(TimerBase*);
  bool updateWidgets();

  bool processUrlFragmentHelper(const String&, UrlFragmentBehavior);
  void setFragmentAnchor(Node*);
  void scrollToFragmentAnchor();
  void didScrollTimerFired(TimerBase*);

  void updateLayersAndCompositingAfterScrollIfNeeded(
      const ScrollOffset& scrollDelta);

  static bool computeCompositedSelection(LocalFrame&, CompositedSelection&);
  void updateCompositedSelectionIfNeeded();

  // Returns true if the FrameView's own scrollbars overlay its content when
  // visible.
  bool hasOverlayScrollbars() const;

  // Returns true if the frame should use custom scrollbars. If true, one of
  // either |customScrollbarElement| or |customScrollbarFrame| will be set to
  // the element or frame which owns the scrollbar with the other set to null.
  bool shouldUseCustomScrollbars(Element*& customScrollbarElement,
                                 LocalFrame*& customScrollbarFrame) const;

  // Returns true if a scrollbar needs to go from native -> custom or vice
  // versa.
  bool needsScrollbarReconstruction() const;

  bool shouldIgnoreOverflowHidden() const;

  void updateScrollCorner();

  AXObjectCache* axObjectCache() const;

  void setLayoutSizeInternal(const IntSize&);

  bool adjustScrollbarExistence(ComputeScrollbarExistenceOption = FirstPass);
  void adjustScrollbarOpacity();
  void adjustScrollOffsetFromUpdateScrollbars();
  bool visualViewportSuppliesScrollbars();

  bool isFrameViewScrollbar(const Widget* child) const {
    return horizontalScrollbar() == child || verticalScrollbar() == child;
  }

  ScrollingCoordinator* scrollingCoordinator() const;

  void prepareLayoutAnalyzer();
  std::unique_ptr<TracedValue> analyzerCounters();

  // LayoutObject for the viewport-defining element (see
  // Document::viewportDefiningElement).
  LayoutObject* viewportLayoutObject() const;

  void collectAnnotatedRegions(LayoutObject&,
                               Vector<AnnotatedRegionValue>&) const;

  template <typename Function>
  void forAllNonThrottledFrameViews(const Function&);

  void setNeedsUpdateViewportIntersection();
  void updateViewportIntersectionsForSubtree(
      DocumentLifecycle::LifecycleState targetState);
  void updateRenderThrottlingStatus(bool hidden, bool subtreeThrottled);
  void notifyResizeObservers();

  // PaintInvalidationCapableScrollableArea
  LayoutScrollbarPart* resizer() const override { return nullptr; }

  bool checkLayoutInvalidationIsAllowed() const;

  PaintController* paintController() { return m_paintController.get(); }

  LayoutSize m_size;

  typedef HashSet<RefPtr<LayoutEmbeddedObject>> EmbeddedObjectSet;
  EmbeddedObjectSet m_partUpdateSet;

  // FIXME: These are just "children" of the FrameView and should be
  // Member<Widget> instead.
  HashSet<RefPtr<LayoutPart>> m_parts;

  // The RefPtr cycle between LocalFrame and FrameView is broken
  // when a LocalFrame is detached by LocalFrame::detach().
  // It clears the LocalFrame's m_view reference via setView(nullptr).
  //
  // For Oilpan, Member reference cycles pose no problem, but
  // LocalFrame's FrameView is also cleared by that setView(), so as to
  // keep the observable lifespan of LocalFrame::view() identical.
  Member<LocalFrame> m_frame;

  WebDisplayMode m_displayMode;

  bool m_canHaveScrollbars;

  bool m_hasPendingLayout;
  LayoutSubtreeRootList m_layoutSubtreeRootList;
  DepthOrderedLayoutObjectList m_orthogonalWritingModeRootList;

  bool m_layoutSchedulingEnabled;
  bool m_inSynchronousPostLayout;
  int m_layoutCount;
  unsigned m_nestedLayoutCount;
  TaskRunnerTimer<FrameView> m_postLayoutTasksTimer;
  TaskRunnerTimer<FrameView> m_updateWidgetsTimer;

  bool m_firstLayout;
  bool m_isTransparent;
  Color m_baseBackgroundColor;
  IntSize m_lastViewportSize;
  float m_lastZoomFactor;

  AtomicString m_mediaType;
  AtomicString m_mediaTypeWhenNotPrinting;

  bool m_safeToPropagateScrollToParent;

  unsigned m_visuallyNonEmptyCharacterCount;
  uint64_t m_visuallyNonEmptyPixelCount;
  bool m_isVisuallyNonEmpty;
  FirstMeaningfulPaintDetector::LayoutObjectCounter m_layoutObjectCounter;

  Member<Node> m_fragmentAnchor;

  // layoutObject to hold our custom scroll corner.
  LayoutScrollbarPart* m_scrollCorner;

  Member<ScrollableAreaSet> m_scrollableAreas;
  Member<ScrollableAreaSet> m_animatingScrollableAreas;
  std::unique_ptr<ResizerAreaSet> m_resizerAreas;
  std::unique_ptr<ViewportConstrainedObjectSet> m_viewportConstrainedObjects;
  unsigned m_stickyPositionObjectCount;
  ViewportConstrainedObjectSet m_backgroundAttachmentFixedObjects;
  Member<FrameViewAutoSizeInfo> m_autoSizeInfo;

  IntSize m_inputEventsOffsetForEmulation;
  float m_inputEventsScaleFactorForEmulation;

  IntSize m_layoutSize;
  IntSize m_initialViewportSize;
  bool m_layoutSizeFixedToFrameSize;

  Timer<FrameView> m_didScrollTimer;

  Vector<IntRect> m_tickmarks;

  float m_browserControlsViewportAdjustment;

  bool m_needsUpdateWidgetGeometries;
  bool m_needsUpdateViewportIntersection;
  bool m_needsUpdateViewportIntersectionInSubtree;

#if ENABLE(ASSERT)
  // Verified when finalizing.
  bool m_hasBeenDisposed;
#endif

  ScrollbarMode m_horizontalScrollbarMode;
  ScrollbarMode m_verticalScrollbarMode;

  bool m_horizontalScrollbarLock;
  bool m_verticalScrollbarLock;

  ChildrenWidgetSet m_children;

  ScrollOffset m_pendingScrollDelta;
  ScrollOffset m_scrollOffset;
  IntSize m_contentsSize;

  bool m_scrollbarsSuppressed;

  bool m_inUpdateScrollbars;

  std::unique_ptr<LayoutAnalyzer> m_analyzer;

  // Mark if something has changed in the mapping from Frame to GraphicsLayer
  // and the Frame Timing regions should be recalculated.
  bool m_frameTimingRequestsDirty;

  // Exists only on root frame.
  // TODO(bokan): crbug.com/484188. We should specialize FrameView for the
  // main frame.
  Member<RootFrameViewport> m_viewportScrollableArea;

  // The following members control rendering pipeline throttling for this
  // frame. They are only updated in response to intersection observer
  // notifications, i.e., not in the middle of the lifecycle.
  bool m_hiddenForThrottling;
  bool m_subtreeThrottled;
  bool m_lifecycleUpdatesThrottled;

  // Paint properties for SPv2 Only.
  // The hierarchy of transform subtree created by a FrameView.
  // [ preTranslation ]               The offset from Widget::frameRect.
  //     |                            Establishes viewport.
  //     +---[ scrollTranslation ]    Frame scrolling.
  // TODO(trchen): These will not be needed once settings->rootLayerScrolls() is
  // enabled.
  RefPtr<TransformPaintPropertyNode> m_preTranslation;
  RefPtr<TransformPaintPropertyNode> m_scrollTranslation;
  RefPtr<ScrollPaintPropertyNode> m_scroll;
  // The content clip clips the document (= LayoutView) but not the scrollbars.
  // TODO(trchen): This will not be needed once settings->rootLayerScrolls() is
  // enabled.
  RefPtr<ClipPaintPropertyNode> m_contentClip;
  // The property tree state that should be used for painting contents. These
  // properties are either created by this FrameView or are inherited from
  // an ancestor.
  std::unique_ptr<PropertyTreeState> m_totalPropertyTreeStateForContents;
  // Whether the paint properties need to be updated. For more details, see
  // FrameView::needsPaintPropertyUpdate().
  bool m_needsPaintPropertyUpdate;

  // This is set on the local root frame view only.
  DocumentLifecycle::LifecycleState m_currentUpdateLifecyclePhasesTargetState;

  ScrollAnchor m_scrollAnchor;
  using AnchoringAdjustmentQueue =
      HeapLinkedHashSet<WeakMember<ScrollableArea>>;
  AnchoringAdjustmentQueue m_anchoringAdjustmentQueue;

  // ScrollbarManager holds the Scrollbar instances.
  ScrollbarManager m_scrollbarManager;

  bool m_needsScrollbarsUpdate;
  bool m_suppressAdjustViewSize;
  bool m_allowsLayoutInvalidationAfterLayoutClean;

  Member<ElementVisibilityObserver> m_visibilityObserver;

  // For testing.
  struct ObjectPaintInvalidation {
    String name;
    PaintInvalidationReason reason;
  };
  std::unique_ptr<Vector<ObjectPaintInvalidation>>
      m_trackedObjectPaintInvalidations;

  // For Slimming Paint v2 only.
  std::unique_ptr<PaintController> m_paintController;
  std::unique_ptr<PaintArtifactCompositor> m_paintArtifactCompositor;
};

inline void FrameView::incrementVisuallyNonEmptyCharacterCount(unsigned count) {
  if (m_isVisuallyNonEmpty)
    return;
  m_visuallyNonEmptyCharacterCount += count;
  // Use a threshold value to prevent very small amounts of visible content from
  // triggering didMeaningfulLayout.  The first few hundred characters rarely
  // contain the interesting content of the page.
  static const unsigned visualCharacterThreshold = 200;
  if (m_visuallyNonEmptyCharacterCount > visualCharacterThreshold)
    setIsVisuallyNonEmpty();
}

inline void FrameView::incrementVisuallyNonEmptyPixelCount(
    const IntSize& size) {
  if (m_isVisuallyNonEmpty)
    return;
  m_visuallyNonEmptyPixelCount += size.area();
  // Use a threshold value to prevent very small amounts of visible content from
  // triggering didMeaningfulLayout.
  static const unsigned visualPixelThreshold = 32 * 32;
  if (m_visuallyNonEmptyPixelCount > visualPixelThreshold)
    setIsVisuallyNonEmpty();
}

DEFINE_TYPE_CASTS(FrameView,
                  Widget,
                  widget,
                  widget->isFrameView(),
                  widget.isFrameView());
DEFINE_TYPE_CASTS(FrameView,
                  ScrollableArea,
                  scrollableArea,
                  scrollableArea->isFrameView(),
                  scrollableArea.isFrameView());

}  // namespace blink

#endif  // FrameView_h
