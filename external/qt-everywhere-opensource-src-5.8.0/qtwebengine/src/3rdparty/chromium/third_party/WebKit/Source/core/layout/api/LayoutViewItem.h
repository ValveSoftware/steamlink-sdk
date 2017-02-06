
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutViewItem_h
#define LayoutViewItem_h

#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutBlockItem.h"

namespace blink {

class PaintLayerCompositor;

class LayoutViewItem : public LayoutBlockItem {
public:
    explicit LayoutViewItem(LayoutView* layoutView)
        : LayoutBlockItem(layoutView)
    {
    }

    explicit LayoutViewItem(const LayoutBlockItem& item)
        : LayoutBlockItem(item)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!item || item.isLayoutView());
    }

    explicit LayoutViewItem(std::nullptr_t) : LayoutBlockItem(nullptr) { }

    LayoutViewItem() { }

    bool usesCompositing() const
    {
        return toView()->usesCompositing();
    }

    PaintLayerCompositor* compositor()
    {
        return toView()->compositor();
    }

    bool hasPendingSelection() const
    {
        return toView()->hasPendingSelection();
    }

    IntRect documentRect() const
    {
        return toView()->documentRect();
    }

    LayoutRect viewRect() const
    {
        return toView()->viewRect();
    }

    IntSize layoutSize(IncludeScrollbarsInRect scrollbars = ExcludeScrollbars) const
    {
        return toView()->layoutSize(scrollbars);
    }

    LayoutRect overflowClipRect(const LayoutPoint& location) const
    {
        return toView()->overflowClipRect(location);
    }

    void clearSelection()
    {
        return toView()->clearSelection();
    }

    bool hitTest(HitTestResult& result)
    {
        return toView()->hitTest(result);
    }

    bool hitTestNoLifecycleUpdate(HitTestResult& result)
    {
        return toView()->hitTestNoLifecycleUpdate(result);
    }

    IntRect selectionBounds()
    {
        return toView()->selectionBounds();
    }

    void invalidatePaintForSelection()
    {
        return toView()->invalidatePaintForSelection();
    }

//    bool hitTest(HitTestResult&);
//    bool hitTestNoLifecycleUpdate(HitTestResult&);

    unsigned hitTestCount() const
    {
        return toView()->hitTestCount();
    }

    unsigned hitTestCacheHits() const
    {
        return toView()->hitTestCacheHits();
    }

    void clearHitTestCache()
    {
        toView()->clearHitTestCache();
    }

    void invalidatePaintForViewAndCompositedLayers()
    {
        toView()->invalidatePaintForViewAndCompositedLayers();
    }

    void sendMediaPositionChangeNotifications(const IntRect& visibleRect)
    {
        toView()->sendMediaPositionChangeNotifications(visibleRect);
    }

    int viewHeight(IncludeScrollbarsInRect scrollbarInclusion = ExcludeScrollbars) const
    {
        return toView()->viewHeight(scrollbarInclusion);
    }

    int viewWidth(IncludeScrollbarsInRect scrollbarInclusion = ExcludeScrollbars) const
    {
        return toView()->viewWidth(scrollbarInclusion);
    }

    FloatSize viewportSizeForViewportUnits() const
    {
        return toView()->viewportSizeForViewportUnits();
    }

private:
    LayoutView* toView() { return toLayoutView(layoutObject()); }
    const LayoutView* toView() const { return toLayoutView(layoutObject()); }
};

} // namespace blink

#endif // LayoutViewItem_h
