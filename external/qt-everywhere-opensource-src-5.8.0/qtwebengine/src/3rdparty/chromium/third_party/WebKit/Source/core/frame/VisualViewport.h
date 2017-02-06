/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VisualViewport_h
#define VisualViewport_h

#include "core/CoreExport.h"
#include "core/events/Event.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "platform/scroll/ScrollableArea.h"
#include "public/platform/WebScrollbar.h"
#include "public/platform/WebSize.h"
#include <memory>

namespace blink {
class WebLayerTreeView;
class WebScrollbarLayer;
}

namespace blink {

class FrameHost;
class GraphicsContext;
class GraphicsLayer;
class IntRect;
class IntSize;
class LocalFrame;

// Represents the visual viewport the user is currently seeing the page through. This
// class corresponds to the InnerViewport on the compositor. It is a ScrollableArea; it's
// offset is set through the GraphicsLayer <-> CC sync mechanisms. Its contents is the page's
// main FrameView, which corresponds to the outer viewport. The inner viewport is always contained
// in the outer viewport and can pan within it.
class CORE_EXPORT VisualViewport final
    : public GarbageCollectedFinalized<VisualViewport>
    , public GraphicsLayerClient
    , public ScrollableArea {
    USING_GARBAGE_COLLECTED_MIXIN(VisualViewport);
public:
    static VisualViewport* create(FrameHost& host)
    {
        return new VisualViewport(host);
    }
    ~VisualViewport() override;

    DECLARE_VIRTUAL_TRACE();

    void attachToLayerTree(GraphicsLayer*);

    GraphicsLayer* rootGraphicsLayer()
    {
        return m_rootTransformLayer.get();
    }
    GraphicsLayer* containerLayer()
    {
        return m_innerViewportContainerLayer.get();
    }
    GraphicsLayer* scrollLayer()
    {
        return m_innerViewportScrollLayer.get();
    }

    void initializeScrollbars();

    // Sets the location of the visual viewport relative to the outer viewport. The
    // coordinates are in partial CSS pixels.
    void setLocation(const FloatPoint&);
    // FIXME: This should be called moveBy
    void move(const FloatPoint&);
    void move(const FloatSize&);
    FloatPoint location() const { return m_offset; }

    // Sets the size of the inner viewport when unscaled in CSS pixels.
    void setSize(const IntSize&);
    IntSize size() const { return m_size; }

    // Gets the scaled size, i.e. the viewport in root view space.
    FloatSize visibleSize() const;

    // Resets the viewport to initial state.
    void reset();

    // Let the viewport know that the main frame changed size (either through screen
    // rotation on Android or window resize elsewhere).
    void mainFrameDidChangeSize();

    // Sets scale and location in one operation, preventing intermediate clamping.
    void setScaleAndLocation(float scale, const FloatPoint& location);
    void setScale(float);
    float scale() const { return m_scale; }

    // Update scale factor, magnifying or minifying by magnifyDelta, centered around
    // the point specified by anchor in window coordinates. Returns false if page
    // scale factor is left unchanged.
    bool magnifyScaleAroundAnchor(float magnifyDelta, const FloatPoint& anchor);

    void registerLayersWithTreeView(WebLayerTreeView*) const;
    void clearLayersForTreeView(WebLayerTreeView*) const;

    // The portion of the unzoomed frame visible in the visual viewport,
    // in partial CSS pixels. Relative to the main frame.
    FloatRect visibleRect() const;

    // The viewport rect relative to the document origin, in partial CSS pixels.
    // FIXME: This should be a DoubleRect since scroll offsets are now doubles.
    FloatRect visibleRectInDocument() const;

    // Convert the given rect in the main FrameView's coordinates into a rect
    // in the viewport. The given and returned rects are in CSS pixels, meaning
    // scale isn't applied.
    FloatRect mainViewToViewportCSSPixels(const FloatRect&) const;
    FloatPoint viewportCSSPixelsToRootFrame(const FloatPoint&) const;

    // Clamp the given point, in document coordinates, to the maximum/minimum
    // scroll extents of the viewport within the document.
    IntPoint clampDocumentOffsetAtScale(const IntPoint& offset, float scale);

    // FIXME: This is kind of a hack. Ideally, we would just resize the
    // viewports to account for top controls. However, FrameView includes much
    // more than just scrolling so we can't simply resize it without incurring
    // all sorts of side-effects. Until we can seperate out the scrollability
    // aspect from FrameView, we use this method to let VisualViewport make the
    // necessary adjustments so that we don't incorrectly clamp scroll offsets
    // coming from the compositor. crbug.com/422328
    void setTopControlsAdjustment(float);

    // Adjust the viewport's offset so that it remains bounded by the outer
    // viepwort.
    void clampToBoundaries();

    FloatRect viewportToRootFrame(const FloatRect&) const;
    IntRect viewportToRootFrame(const IntRect&) const;
    FloatRect rootFrameToViewport(const FloatRect&) const;
    IntRect rootFrameToViewport(const IntRect&) const;

    FloatPoint viewportToRootFrame(const FloatPoint&) const;
    FloatPoint rootFrameToViewport(const FloatPoint&) const;
    IntPoint viewportToRootFrame(const IntPoint&) const;
    IntPoint rootFrameToViewport(const IntPoint&) const;

    // ScrollableArea implementation
    HostWindow* getHostWindow() const override;
    DoubleRect visibleContentRectDouble(IncludeScrollbarsInRect = ExcludeScrollbars) const override;
    IntRect visibleContentRect(IncludeScrollbarsInRect = ExcludeScrollbars) const override;
    bool shouldUseIntegerScrollOffset() const override;
    LayoutRect visualRectForScrollbarParts() const override { ASSERT_NOT_REACHED(); return LayoutRect(); }
    bool isActive() const override { return false; }
    int scrollSize(ScrollbarOrientation) const override;
    bool isScrollCornerVisible() const override { return false; }
    IntRect scrollCornerRect() const override { return IntRect(); }
    IntPoint scrollPosition() const override { return flooredIntPoint(m_offset); }
    DoublePoint scrollPositionDouble() const override { return m_offset; }
    IntPoint minimumScrollPosition() const override;
    IntPoint maximumScrollPosition() const override;
    DoublePoint maximumScrollPositionDouble() const override;
    int visibleHeight() const override { return visibleRect().height(); }
    int visibleWidth() const override { return visibleRect().width(); }
    IntSize contentsSize() const override;
    bool scrollbarsCanBeActive() const override { return false; }
    IntRect scrollableAreaBoundingBox() const override;
    bool userInputScrollable(ScrollbarOrientation) const override { return true; }
    bool shouldPlaceVerticalScrollbarOnLeft() const override { return false; }
    bool scrollAnimatorEnabled() const override;
    void scrollControlWasSetNeedsPaintInvalidation() override { }
    void setScrollOffset(const DoublePoint&, ScrollType) override;
    GraphicsLayer* layerForContainer() const override;
    GraphicsLayer* layerForScrolling() const override;
    GraphicsLayer* layerForHorizontalScrollbar() const override;
    GraphicsLayer* layerForVerticalScrollbar() const override;
    Widget* getWidget() override;
    CompositorAnimationTimeline* compositorAnimationTimeline() const override;

    // Visual Viewport API implementation.
    double scrollLeft();
    double scrollTop();
    double clientWidth();
    double clientHeight();
    double pageScale();

    // Used for gathering data on user pinch-zoom statistics.
    void userDidChangeScale();
    void sendUMAMetrics();
    void startTrackingPinchStats();

    // Heuristic-based function for determining if we should disable workarounds
    // for viewing websites that are not optimized for mobile devices.
    bool shouldDisableDesktopWorkarounds() const;

private:
    explicit VisualViewport(FrameHost&);

    bool visualViewportSuppliesScrollbars() const;

    void updateStyleAndLayoutIgnorePendingStylesheets();

    void enqueueScrollEvent();
    void enqueueResizeEvent();

    // GraphicsLayerClient implementation.
    bool needsRepaint(const GraphicsLayer&) const { ASSERT_NOT_REACHED(); return true; }
    IntRect computeInterestRect(const GraphicsLayer*, const IntRect&) const;
    void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect&) const override;
    String debugName(const GraphicsLayer*) const override;

    void setupScrollbar(WebScrollbar::Orientation);
    FloatPoint clampOffsetToBoundaries(const FloatPoint&);

    LocalFrame* mainFrame() const;

    FrameHost& frameHost() const
    {
        ASSERT(m_frameHost);
        return *m_frameHost;
    }

    Member<FrameHost> m_frameHost;
    std::unique_ptr<GraphicsLayer> m_rootTransformLayer;
    std::unique_ptr<GraphicsLayer> m_innerViewportContainerLayer;
    std::unique_ptr<GraphicsLayer> m_overscrollElasticityLayer;
    std::unique_ptr<GraphicsLayer> m_pageScaleLayer;
    std::unique_ptr<GraphicsLayer> m_innerViewportScrollLayer;
    std::unique_ptr<GraphicsLayer> m_overlayScrollbarHorizontal;
    std::unique_ptr<GraphicsLayer> m_overlayScrollbarVertical;
    std::unique_ptr<WebScrollbarLayer> m_webOverlayScrollbarHorizontal;
    std::unique_ptr<WebScrollbarLayer> m_webOverlayScrollbarVertical;

    // Offset of the visual viewport from the main frame's origin, in CSS pixels.
    FloatPoint m_offset;
    float m_scale;
    IntSize m_size;
    float m_topControlsAdjustment;
    float m_maxPageScale;
    bool m_trackPinchZoomStatsForPage;
};

} // namespace blink

#endif // VisualViewport_h
