/*
 * Copyright (C) 2003, 2009, 2012 Apple Inc. All rights reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#ifndef PaintLayerScrollableArea_h
#define PaintLayerScrollableArea_h

#include "core/CoreExport.h"
#include "core/layout/ScrollAnchor.h"
#include "core/layout/ScrollEnums.h"
#include "core/paint/PaintInvalidationCapableScrollableArea.h"
#include "core/paint/PaintLayerFragment.h"
#include "platform/heap/Handle.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

enum ResizerHitTestType {
    ResizerForPointer,
    ResizerForTouch
};

class ComputedStyle;
class HitTestResult;
class LayoutBox;
class LayoutScrollbarPart;
class PaintLayer;
class PlatformEvent;
class StickyPositionScrollingConstraints;
class SubtreeLayoutScope;

typedef WTF::HashMap<PaintLayer*, StickyPositionScrollingConstraints> StickyConstraintsMap;

struct PaintLayerScrollableAreaRareData {
    WTF_MAKE_NONCOPYABLE(PaintLayerScrollableAreaRareData);
    USING_FAST_MALLOC(PaintLayerScrollableAreaRareData);
public:
    PaintLayerScrollableAreaRareData();

    StickyConstraintsMap m_stickyConstraintsMap;
};

// PaintLayerScrollableArea represents the scrollable area of a LayoutBox.
//
// To be scrollable, an element requires ‘overflow’ != visible. Note that this
// doesn’t imply having scrollbars as you can always programmatically scroll
// when ‘overflow’ is hidden (using JavaScript's element.scrollTo or
// scrollLeft).
//
// The size and scroll origin of the scrollable area are based on layout
// dimensions. They are recomputed after layout in updateScrollDimensions.
//
// updateScrollDimensions also determines if scrollbars need to be allocated,
// destroyed or updated as a result of layout. This is based on the value of the
// 'overflow' property. Having non-overlay scrollbars automatically allocates a
// scrollcorner (m_scrollCorner), which is used to style the intersection of the
// two scrollbars.
//
// Note that scrollbars are placed based on the LayoutBox's computed
// 'direction'. See https://webkit.org/b/54623 for some context.
//
// The ‘resize' property allocates a resizer (m_resizer), which is overlaid on
// top of the scroll corner. It is used to resize an element using the mouse.
//
// The scrollbars and scroll corner can also be hardware accelerated
// and thus get their own GraphicsLayer (see the layerFor* functions).
// This only happens if the associated PaintLayer is itself composited.
//
//
// ***** OVERLAY SCROLLBARS *****
// Overlay scrollbars are painted on top of the box's content. As such they
// don't use any space in the box. Software overlay scrollbars are painted by
// PaintLayerPainter::paintOverlayScrollbars after all content as part of a
// separate tree traversal. The reason for this 2nd traversal is that they need
// to be painted on top of everything. Hardware accelerated overlay scrollbars
// are painted by their associated GraphicsLayer that sets the paint flag
// PaintLayerPaintingOverlayScrollbars.
class CORE_EXPORT PaintLayerScrollableArea final : public GarbageCollectedFinalized<PaintLayerScrollableArea>, public PaintInvalidationCapableScrollableArea {
    USING_GARBAGE_COLLECTED_MIXIN(PaintLayerScrollableArea);
    friend class Internals;

private:
    class ScrollbarManager {
        DISALLOW_NEW();

        // Helper class to manage the life cycle of Scrollbar objects.  Some layout containers
        // (e.g., flexbox, table) run multi-pass layout on their children, applying different
        // constraints.  If a child has overflow:auto, it may gain and lose scrollbars multiple
        // times during multi-pass layout, causing pointless allocation/deallocation thrashing,
        // and potentially leading to other problems (crbug.com/528940).

        // ScrollbarManager allows a ScrollableArea to delay the destruction of a scrollbar that
        // is no longer needed, until the end of multi-pass layout.  If the scrollbar is then
        // re-added before multi-pass layout finishes, the previously "deleted" scrollbar will
        // be restored, rather than constructing a new one.
    public:
        ScrollbarManager(PaintLayerScrollableArea&);

        void dispose();

        Scrollbar* horizontalScrollbar() const { return m_hBarIsAttached ? m_hBar.get(): nullptr; }
        Scrollbar* verticalScrollbar() const { return m_vBarIsAttached ? m_vBar.get() : nullptr; }
        bool hasHorizontalScrollbar() const { return horizontalScrollbar(); }
        bool hasVerticalScrollbar() const { return verticalScrollbar(); }

        void setHasHorizontalScrollbar(bool hasScrollbar);
        void setHasVerticalScrollbar(bool hasScrollbar);
        void destroyDetachedScrollbars();

        DECLARE_TRACE();

    private:
        Scrollbar* createScrollbar(ScrollbarOrientation);
        void destroyScrollbar(ScrollbarOrientation);

    private:
        Member<PaintLayerScrollableArea> m_scrollableArea;

        // The scrollbars associated with m_scrollableArea. Both can nullptr.
        Member<Scrollbar> m_hBar;
        Member<Scrollbar> m_vBar;

        unsigned m_hBarIsAttached: 1;
        unsigned m_vBarIsAttached: 1;
    };

public:

    // If a PreventRelayoutScope object is alive, updateAfterLayout() will not
    // re-run box layout as a result of adding or removing scrollbars.
    // Instead, it will mark the PLSA as needing relayout of its box.
    // When the last PreventRelayoutScope object is popped off the stack,
    // box().setNeedsLayout(), and box().scrollbarsChanged() for LayoutBlock's,
    // will be called as appropriate for all marked PLSA's.
    class PreventRelayoutScope {
        STACK_ALLOCATED();
    public:
        PreventRelayoutScope(SubtreeLayoutScope&);
        ~PreventRelayoutScope();

        static bool relayoutIsPrevented() { return s_count; }
        static void setBoxNeedsLayout(PaintLayerScrollableArea&, bool hadHorizontalScrollbar, bool hadVerticalScrollbar);
        static bool relayoutNeeded() { return s_count == 0 && s_relayoutNeeded; }
        static void resetRelayoutNeeded();

    private:
        static int s_count;
        static SubtreeLayoutScope* s_layoutScope;
        static bool s_relayoutNeeded;
        static PersistentHeapVector<Member<PaintLayerScrollableArea>>* s_needsRelayout;
    };

    // If a FreezeScrollbarScope object is alive, updateAfterLayout() will not
    // recompute the existence of overflow:auto scrollbars.
    class FreezeScrollbarsScope {
        STACK_ALLOCATED();
    public:
        FreezeScrollbarsScope() { s_count++; }
        ~FreezeScrollbarsScope() { s_count--; }

        static bool scrollbarsAreFrozen() { return s_count; }

    private:
        static int s_count;
    };

    // If a DelayScrollPositionClampScope object is alive, updateAfterLayout() will not
    // clamp scroll positions to ensure they are in the valid range.  When
    // the last DelayScrollPositionClampScope object is destructed, all PaintLayerScrollableArea's
    // that delayed clamping their positions will immediately clamp them.
    class DelayScrollPositionClampScope {
        STACK_ALLOCATED();
    public:
        DelayScrollPositionClampScope();
        ~DelayScrollPositionClampScope();

        static bool clampingIsDelayed() { return s_count; }
        static void setNeedsClamp(PaintLayerScrollableArea*);

    private:
        static void clampScrollableAreas();

        static int s_count;
        static PersistentHeapVector<Member<PaintLayerScrollableArea>>* s_needsClamp;
    };

    // FIXME: We should pass in the LayoutBox but this opens a window
    // for crashers during PaintLayer setup (see crbug.com/368062).
    static PaintLayerScrollableArea* create(PaintLayer& layer)
    {
        return new PaintLayerScrollableArea(layer);
    }

    ~PaintLayerScrollableArea() override;
    void dispose();

    bool hasHorizontalScrollbar() const { return horizontalScrollbar(); }
    bool hasVerticalScrollbar() const { return verticalScrollbar(); }

    Scrollbar* horizontalScrollbar() const override { return m_scrollbarManager.horizontalScrollbar(); }
    Scrollbar* verticalScrollbar() const override { return m_scrollbarManager.verticalScrollbar(); }

    HostWindow* getHostWindow() const override;

    // For composited scrolling, we allocate an extra GraphicsLayer to hold
    // onto the scrolling content. The layer can be shifted on the GPU and
    // composited at little cost.
    // Note that this is done in CompositedLayerMapping, this function being
    // only a helper.
    GraphicsLayer* layerForScrolling() const override;

    // GraphicsLayers for the scrolling components.
    //
    // Any function can return nullptr if they are not accelerated.
    GraphicsLayer* layerForHorizontalScrollbar() const override;
    GraphicsLayer* layerForVerticalScrollbar() const override;
    GraphicsLayer* layerForScrollCorner() const override;

    bool usesCompositedScrolling() const override;
    bool shouldScrollOnMainThread() const override;
    bool shouldUseIntegerScrollOffset() const override;
    bool isActive() const override;
    bool isScrollCornerVisible() const override;
    IntRect scrollCornerRect() const override;
    IntRect convertFromScrollbarToContainingWidget(const Scrollbar&, const IntRect&) const override;
    IntRect convertFromContainingWidgetToScrollbar(const Scrollbar&, const IntRect&) const override;
    IntPoint convertFromScrollbarToContainingWidget(const Scrollbar&, const IntPoint&) const override;
    IntPoint convertFromContainingWidgetToScrollbar(const Scrollbar&, const IntPoint&) const override;
    int scrollSize(ScrollbarOrientation) const override;
    IntPoint scrollPosition() const override;
    DoublePoint scrollPositionDouble() const override;
    IntPoint minimumScrollPosition() const override;
    IntPoint maximumScrollPosition() const override;
    IntRect visibleContentRect(IncludeScrollbarsInRect = ExcludeScrollbars) const override;
    int visibleHeight() const override;
    int visibleWidth() const override;
    IntSize contentsSize() const override;
    IntPoint lastKnownMousePosition() const override;
    bool scrollAnimatorEnabled() const override;
    bool shouldSuspendScrollAnimations() const override;
    bool scrollbarsCanBeActive() const override;
    void scrollbarVisibilityChanged() override;
    IntRect scrollableAreaBoundingBox() const override;
    void registerForAnimation() override;
    void deregisterForAnimation() override;
    bool userInputScrollable(ScrollbarOrientation) const override;
    bool shouldPlaceVerticalScrollbarOnLeft() const override;
    int pageStep(ScrollbarOrientation) const override;
    ScrollBehavior scrollBehaviorStyle() const override;
    CompositorAnimationTimeline* compositorAnimationTimeline() const override;

    double scrollXOffset() const { return m_scrollOffset.width() + scrollOrigin().x(); }
    double scrollYOffset() const { return m_scrollOffset.height() + scrollOrigin().y(); }

    DoubleSize scrollOffset() const { return m_scrollOffset; }

    // FIXME: We shouldn't allow access to m_overflowRect outside this class.
    LayoutRect overflowRect() const { return m_overflowRect; }

    void scrollToPosition(const DoublePoint& scrollPosition, ScrollOffsetClamping = ScrollOffsetUnclamped,
        ScrollBehavior = ScrollBehaviorInstant, ScrollType = ProgrammaticScroll);

    void scrollToOffset(const DoubleSize& scrollOffset, ScrollOffsetClamping clamp = ScrollOffsetUnclamped,
        ScrollBehavior scrollBehavior = ScrollBehaviorInstant, ScrollType scrollType = ProgrammaticScroll)
    {
        scrollToPosition(-scrollOrigin() + scrollOffset, clamp, scrollBehavior, scrollType);
    }

    void scrollToXOffset(double x, ScrollOffsetClamping clamp = ScrollOffsetUnclamped, ScrollBehavior scrollBehavior = ScrollBehaviorInstant)
    {
        scrollToOffset(DoubleSize(x, scrollYOffset()), clamp, scrollBehavior);
    }

    void scrollToYOffset(double y, ScrollOffsetClamping clamp = ScrollOffsetUnclamped, ScrollBehavior scrollBehavior = ScrollBehaviorInstant)
    {
        scrollToOffset(DoubleSize(scrollXOffset(), y), clamp, scrollBehavior);
    }

    void setScrollPosition(const DoublePoint& position, ScrollType scrollType, ScrollBehavior scrollBehavior = ScrollBehaviorInstant) override
    {
        scrollToOffset(toDoubleSize(position), ScrollOffsetClamped, scrollBehavior, scrollType);
    }

    // TODO(szager): Actually run these after all of layout is finished.  Currently, they
    // run at the end of box()'es layout (or after all flexbox layout has finished) but while
    // document layout is still happening.
    void updateAfterLayout();
    void clampScrollPositionsAfterLayout();

    void updateAfterStyleChange(const ComputedStyle*);
    void updateAfterOverflowRecalc();

    bool updateAfterCompositingChange() override;

    bool hasScrollbar() const { return hasHorizontalScrollbar() || hasVerticalScrollbar(); }
    bool hasOverflowControls() const { return hasScrollbar() || scrollCorner() || resizer(); }

    LayoutScrollbarPart* scrollCorner() const override { return m_scrollCorner; }

    void resize(const PlatformEvent&, const LayoutSize&);
    IntSize offsetFromResizeCorner(const IntPoint& absolutePoint) const;

    bool inResizeMode() const { return m_inResizeMode; }
    void setInResizeMode(bool inResizeMode) { m_inResizeMode = inResizeMode; }

    IntRect touchResizerCornerRect(const IntRect& bounds) const
    {
        return resizerCornerRect(bounds, ResizerForTouch);
    }

    LayoutUnit scrollWidth() const;
    LayoutUnit scrollHeight() const;
    int pixelSnappedScrollWidth() const;
    int pixelSnappedScrollHeight() const;

    int verticalScrollbarWidth(OverlayScrollbarClipBehavior = IgnoreOverlayScrollbarSize) const;
    int horizontalScrollbarHeight(OverlayScrollbarClipBehavior = IgnoreOverlayScrollbarSize) const;

    DoubleSize adjustedScrollOffset() const { return DoubleSize(scrollXOffset(), scrollYOffset()); }

    void positionOverflowControls();

    // isPointInResizeControl() is used for testing if a pointer/touch position is in the resize control
    // area.
    bool isPointInResizeControl(const IntPoint& absolutePoint, ResizerHitTestType) const;
    bool hitTestOverflowControls(HitTestResult&, const IntPoint& localPoint);

    bool hitTestResizerInFragments(const PaintLayerFragments&, const HitTestLocation&) const;

    LayoutRect scrollIntoView(const LayoutRect&, const ScrollAlignment& alignX, const ScrollAlignment& alignY, ScrollType = ProgrammaticScroll) override;

    // Returns true if scrollable area is in the FrameView's collection of scrollable areas. This can
    // only happen if we're scrollable, visible to hit test, and do in fact overflow. This means that
    // 'overflow: hidden' or 'pointer-events: none' layers never get added to the FrameView's collection.
    bool scrollsOverflow() const { return m_scrollsOverflow; }

    // Rectangle encompassing the scroll corner and resizer rect.
    IntRect scrollCornerAndResizerRect() const final;

    enum LCDTextMode {
        ConsiderLCDText,
        IgnoreLCDText
    };

    void updateNeedsCompositedScrolling(LCDTextMode = ConsiderLCDText);
    bool needsCompositedScrolling() const { return m_needsCompositedScrolling; }

    // These are used during compositing updates to determine if the overflow
    // controls need to be repositioned in the GraphicsLayer tree.
    void setTopmostScrollChild(PaintLayer*);
    PaintLayer* topmostScrollChild() const { ASSERT(!m_nextTopmostScrollChild); return m_topmostScrollChild; }

    IntRect resizerCornerRect(const IntRect&, ResizerHitTestType) const;

    LayoutBox& box() const;
    PaintLayer* layer() const;

    LayoutScrollbarPart* resizer() const override { return m_resizer; }

    const IntPoint& cachedOverlayScrollbarOffset() { return m_cachedOverlayScrollbarOffset; }
    void setCachedOverlayScrollbarOffset(const IntPoint& offset) { m_cachedOverlayScrollbarOffset = offset; }

    IntRect rectForHorizontalScrollbar(const IntRect& borderBoxRect) const;
    IntRect rectForVerticalScrollbar(const IntRect& borderBoxRect) const;

    Widget* getWidget() override;
    ScrollAnchor& scrollAnchor() { return m_scrollAnchor; }
    bool isPaintLayerScrollableArea() const override { return true; }

    bool shouldRebuildHorizontalScrollbarLayer() const { return m_rebuildHorizontalScrollbarLayer; }
    bool shouldRebuildVerticalScrollbarLayer() const { return m_rebuildVerticalScrollbarLayer; }
    void resetRebuildScrollbarLayerFlags();

    // Did DelayScrollPositionClampScope prevent us from running clampScrollPositionsAfterLayout()
    // in updateAfterLayout()?
    bool needsScrollPositionClamp() const { return m_needsScrollPositionClamp; }
    void setNeedsScrollPositionClamp(bool val) { m_needsScrollPositionClamp = val; }

    // Did PreventRelayoutScope prevent us from running re-layout due to adding/subtracting
    // scrollbars in updateAfterLayout()?
    bool needsRelayout() const { return m_needsRelayout; }
    void setNeedsRelayout(bool val) { m_needsRelayout = val; }

    // Were we laid out with a horizontal scrollbar at the time we were marked as
    // needing relayout by PreventRelayoutScope?
    bool hadHorizontalScrollbarBeforeRelayout() const { return m_hadHorizontalScrollbarBeforeRelayout; }
    void setHadHorizontalScrollbarBeforeRelayout(bool val) { m_hadHorizontalScrollbarBeforeRelayout = val; }

    // Were we laid out with a vertical scrollbar at the time we were marked as
    // needing relayout by PreventRelayoutScope?
    bool hadVerticalScrollbarBeforeRelayout() const { return m_hadVerticalScrollbarBeforeRelayout; }
    void setHadVerticalScrollbarBeforeRelayout(bool val) { m_hadVerticalScrollbarBeforeRelayout = val; }

    StickyConstraintsMap& stickyConstraintsMap() { return ensureRareData().m_stickyConstraintsMap; }
    void invalidateAllStickyConstraints();
    void invalidateStickyConstraintsFor(PaintLayer*, bool needsCompositingUpdate = true);

    uint64_t id() const;

    DECLARE_VIRTUAL_TRACE();

private:
    explicit PaintLayerScrollableArea(PaintLayer&);

    bool hasHorizontalOverflow() const;
    bool hasVerticalOverflow() const;
    bool hasScrollableHorizontalOverflow() const;
    bool hasScrollableVerticalOverflow() const;
    bool visualViewportSuppliesScrollbars() const;

    bool needsScrollbarReconstruction() const;

    void updateScrollOrigin();
    void updateScrollDimensions();

    void setScrollOffset(const DoublePoint&, ScrollType) override;

    int verticalScrollbarStart(int minX, int maxX) const;
    int horizontalScrollbarStart(int minX) const;
    IntSize scrollbarOffset(const Scrollbar&) const;

    void setHasHorizontalScrollbar(bool hasScrollbar);
    void setHasVerticalScrollbar(bool hasScrollbar);

    void updateScrollCornerStyle();

    // See comments on isPointInResizeControl.
    void updateResizerAreaSet();
    void updateResizerStyle();


    void updateScrollableAreaSet(bool hasOverflow);

    void updateCompositingLayersAfterScroll();

    PaintLayerScrollableAreaRareData* rareData()
    {
        return m_rareData.get();
    }

    PaintLayerScrollableAreaRareData& ensureRareData()
    {
        if (!m_rareData)
            m_rareData = wrapUnique(new PaintLayerScrollableAreaRareData());
        return *m_rareData.get();
    }

    // PaintInvalidationCapableScrollableArea
    LayoutBox& boxForScrollControlPaintInvalidation() const { return box(); }

    PaintLayer& m_layer;

    PaintLayer* m_nextTopmostScrollChild;
    PaintLayer* m_topmostScrollChild;

    // Keeps track of whether the layer is currently resizing, so events can cause resizing to start and stop.
    unsigned m_inResizeMode : 1;
    unsigned m_scrollsOverflow : 1;

    unsigned m_inOverflowRelayout : 1;

    // FIXME: once cc can handle composited scrolling with clip paths, we will
    // no longer need this bit.
    unsigned m_needsCompositedScrolling : 1;

    // Set to indicate that a scrollbar layer, if present, needs to be rebuilt
    // in the next compositing update because the underlying blink::Scrollbar
    // instance has been reconstructed.
    unsigned m_rebuildHorizontalScrollbarLayer : 1;
    unsigned m_rebuildVerticalScrollbarLayer : 1;

    unsigned m_needsScrollPositionClamp : 1;
    unsigned m_needsRelayout : 1;
    unsigned m_hadHorizontalScrollbarBeforeRelayout : 1;
    unsigned m_hadVerticalScrollbarBeforeRelayout : 1;

    // The width/height of our scrolled area.
    // This is OverflowModel's layout overflow translated to physical
    // coordinates. See OverflowModel for the different overflow and
    // LayoutBoxModelObject for the coordinate systems.
    LayoutRect m_overflowRect;

    // ScrollbarManager holds the Scrollbar instances.
    ScrollbarManager m_scrollbarManager;

    // This is the (scroll) offset from scrollOrigin().
    DoubleSize m_scrollOffset;

    IntPoint m_cachedOverlayScrollbarOffset;

    // LayoutObject to hold our custom scroll corner.
    LayoutScrollbarPart* m_scrollCorner;

    // LayoutObject to hold our custom resizer.
    LayoutScrollbarPart* m_resizer;

    ScrollAnchor m_scrollAnchor;

    std::unique_ptr<PaintLayerScrollableAreaRareData> m_rareData;

#if ENABLE(ASSERT)
    bool m_hasBeenDisposed;
#endif
};

DEFINE_TYPE_CASTS(PaintLayerScrollableArea, ScrollableArea, scrollableArea,
    scrollableArea->isPaintLayerScrollableArea(),
    scrollableArea.isPaintLayerScrollableArea());

} // namespace blink

#endif // LayerScrollableArea_h
