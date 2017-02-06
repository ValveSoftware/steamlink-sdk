// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SnapCoordinator_h
#define SnapCoordinator_h

#include "core/CoreExport.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class ComputedStyle;
class ContainerNode;
class Element;
class LayoutBox;
struct LengthPoint;

// Snap Coordinator keeps track of snap containers and all of their associated
// snap areas. It also contains the logic to generate the list of valid snap
// positions for a given snap container.
//
// Snap container:
//   An scroll container that has 'scroll-snap-type' value other
//   than 'none'.
// Snap area:
//   A snap container's descendant that contributes snap positions. An element
//   only contributes snap positions to its nearest ancestor (on the element’s
//   containing block chain) scroll container.
//
// For more information see spec: https://drafts.csswg.org/css-snappoints/
class CORE_EXPORT SnapCoordinator final : public GarbageCollectedFinalized<SnapCoordinator> {
    WTF_MAKE_NONCOPYABLE(SnapCoordinator);

public:
    static SnapCoordinator* create();
    ~SnapCoordinator();
    DEFINE_INLINE_TRACE() {}

    void snapContainerDidChange(LayoutBox&, ScrollSnapType);
    void snapAreaDidChange(LayoutBox&, const Vector<LengthPoint>& snapCoordinates);

#ifndef NDEBUG
    void showSnapAreaMap();
    void showSnapAreasFor(const LayoutBox*);
#endif

private:
    friend class SnapCoordinatorTest;
    explicit SnapCoordinator();

    Vector<double> snapOffsets(const ContainerNode&, ScrollbarOrientation);

    HashSet<const LayoutBox*> m_snapContainers;
};

} // namespace blink

#endif // SnapCoordinator_h
