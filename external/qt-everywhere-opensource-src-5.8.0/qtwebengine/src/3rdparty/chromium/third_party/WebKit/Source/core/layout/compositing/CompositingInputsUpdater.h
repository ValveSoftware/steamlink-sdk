// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositingInputsUpdater_h
#define CompositingInputsUpdater_h

#include "core/layout/LayoutGeometryMap.h"
#include "wtf/Allocator.h"

namespace blink {

class PaintLayer;

class CompositingInputsUpdater {
    STACK_ALLOCATED();
public:
    explicit CompositingInputsUpdater(PaintLayer* rootLayer);
    ~CompositingInputsUpdater();

    void update();

#if ENABLE(ASSERT)
    static void assertNeedsCompositingInputsUpdateBitsCleared(PaintLayer*);
#endif

private:
    enum UpdateType {
        DoNotForceUpdate,
        ForceUpdate,
    };

    struct AncestorInfo {
        AncestorInfo()
            : ancestorStackingContext(nullptr)
            , enclosingCompositedLayer(nullptr)
            , lastOverflowClipLayer(nullptr)
            , lastScrollingAncestor(nullptr)
            , hasAncestorWithClipRelatedProperty(false)
            , hasAncestorWithClipPath(false)
        {
        }

        PaintLayer* ancestorStackingContext;
        PaintLayer* enclosingCompositedLayer;
        PaintLayer* lastOverflowClipLayer;
        // Notice that lastScrollingAncestor isn't the same thing as
        // ancestorScrollingLayer. The former is just the nearest scrolling
        // along the PaintLayer::parent() chain. The latter is the layer that
        // actually controls the scrolling of this layer, which we find on the
        // containing block chain.
        PaintLayer* lastScrollingAncestor;
        bool hasAncestorWithClipRelatedProperty;
        bool hasAncestorWithClipPath;
    };

    void updateRecursive(PaintLayer*, UpdateType, AncestorInfo);

    LayoutGeometryMap m_geometryMap;
    PaintLayer* m_rootLayer;
};

} // namespace blink

#endif // CompositingInputsUpdater_h
