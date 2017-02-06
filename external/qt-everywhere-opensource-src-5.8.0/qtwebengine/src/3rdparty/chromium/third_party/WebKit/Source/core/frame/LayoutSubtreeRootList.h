// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutSubtreeRootList_h
#define LayoutSubtreeRootList_h

#include "core/layout/DepthOrderedLayoutObjectList.h"
#include "wtf/Allocator.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"

namespace blink {

// This class keeps track of layout objects that have identified to be
// independent layout roots meaning they won't affect other parts of the tree
// by their layout. This is an optimization to avoid doing extra work and tree
// walking during layout. See objectIsRelayoutBoundary for the criteria for
// being a root.
// These roots are sorted into a vector ordered by their depth in the tree,
// and returned one by one deepest first for layout. This is necessary in the
// case of nested subtree roots where a positioned object is added to the
// contained root but its containing block is above that root.
// It ensures we add positioned objects to their containing block's positioned
// descendant lists before laying out those objects if they're contained in
// a higher root.
// TODO(leviw): This should really be something akin to a LayoutController
// that FrameView delegates layout work to.
class LayoutSubtreeRootList : public DepthOrderedLayoutObjectList {
    DISALLOW_NEW();
public:
    LayoutSubtreeRootList()
    { }

    void clearAndMarkContainingBlocksForLayout();

    // TODO(leviw): Remove this once we stop exposing to DevTools one root
    // for a layout crbug.com/460596
    LayoutObject* randomRoot();

    void countObjectsNeedingLayout(unsigned& needsLayoutObjects, unsigned& totalObjects);

    static void countObjectsNeedingLayoutInRoot(const LayoutObject* root, unsigned& needsLayoutObjects, unsigned& totalObjects);
};

} // namespace blink

#endif // LayoutSubtreeRootList_h
