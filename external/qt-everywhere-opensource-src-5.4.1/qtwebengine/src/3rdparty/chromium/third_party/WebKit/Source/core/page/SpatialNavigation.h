/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Antonio Gomes <tonikitoo@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SpatialNavigation_h
#define SpatialNavigation_h

#include "core/dom/Node.h"
#include "core/page/FocusType.h"
#include "platform/geometry/LayoutRect.h"

#include <limits>

namespace WebCore {

class Element;
class LocalFrame;
class HTMLAreaElement;
class HTMLFrameOwnerElement;
class IntRect;
class RenderObject;

inline long long maxDistance()
{
    return std::numeric_limits<long long>::max();
}

inline int fudgeFactor()
{
    return 2;
}

bool isSpatialNavigationEnabled(const LocalFrame*);

// Spatially speaking, two given elements in a web page can be:
// 1) Fully aligned: There is a full intersection between the rects, either
//    vertically or horizontally.
//
// * Horizontally       * Vertically
//    _
//   |_|                   _ _ _ _ _ _
//   |_|...... _          |_|_|_|_|_|_|
//   |_|      |_|         .       .
//   |_|......|_|   OR    .       .
//   |_|      |_|         .       .
//   |_|......|_|          _ _ _ _
//   |_|                  |_|_|_|_|
//
//
// 2) Partially aligned: There is a partial intersection between the rects, either
//    vertically or horizontally.
//
// * Horizontally       * Vertically
//    _                   _ _ _ _ _
//   |_|                 |_|_|_|_|_|
//   |_|.... _      OR           . .
//   |_|    |_|                  . .
//   |_|....|_|                  ._._ _
//          |_|                  |_|_|_|
//          |_|
//
// 3) Or, otherwise, not aligned at all.
//
// * Horizontally       * Vertically
//         _              _ _ _ _
//        |_|            |_|_|_|_|
//        |_|                    .
//        |_|                     .
//       .          OR             .
//    _ .                           ._ _ _ _ _
//   |_|                            |_|_|_|_|_|
//   |_|
//   |_|
//
// "Totally Aligned" elements are preferable candidates to move
// focus to over "Partially Aligned" ones, that on its turns are
// more preferable than "Not Aligned".
enum RectsAlignment {
    None = 0,
    Partial,
    Full
};

struct FocusCandidate {
    FocusCandidate()
        : visibleNode(0)
        , focusableNode(0)
        , enclosingScrollableBox(0)
        , distance(maxDistance())
        , parentDistance(maxDistance())
        , alignment(None)
        , parentAlignment(None)
        , isOffscreen(true)
        , isOffscreenAfterScrolling(true)
    {
    }

    FocusCandidate(Node*, FocusType);
    explicit FocusCandidate(HTMLAreaElement*, FocusType);
    bool isNull() const { return !visibleNode; }
    bool inScrollableContainer() const { return visibleNode && enclosingScrollableBox; }
    bool isFrameOwnerElement() const { return visibleNode && visibleNode->isFrameOwnerElement(); }
    Document* document() const { return visibleNode ? &visibleNode->document() : 0; }

    // We handle differently visibleNode and FocusableNode to properly handle the areas of imagemaps,
    // where visibleNode would represent the image element and focusableNode would represent the area element.
    // In all other cases, visibleNode and focusableNode are one and the same.
    Node* visibleNode;
    Node* focusableNode;
    Node* enclosingScrollableBox;
    long long distance;
    long long parentDistance;
    RectsAlignment alignment;
    RectsAlignment parentAlignment;
    LayoutRect rect;
    bool isOffscreen;
    bool isOffscreenAfterScrolling;
};

bool hasOffscreenRect(Node*, FocusType = FocusTypeNone);
bool scrollInDirection(LocalFrame*, FocusType);
bool scrollInDirection(Node* container, FocusType);
bool canScrollInDirection(const Node* container, FocusType);
bool canScrollInDirection(const LocalFrame*, FocusType);
bool canBeScrolledIntoView(FocusType, const FocusCandidate&);
bool areElementsOnSameLine(const FocusCandidate& firstCandidate, const FocusCandidate& secondCandidate);
void distanceDataForNode(FocusType, const FocusCandidate& current, FocusCandidate&);
Node* scrollableEnclosingBoxOrParentFrameForNodeInDirection(FocusType, Node*);
LayoutRect nodeRectInAbsoluteCoordinates(Node*, bool ignoreBorder = false);
LayoutRect frameRectInAbsoluteCoordinates(LocalFrame*);
LayoutRect virtualRectForDirection(FocusType, const LayoutRect& startingRect, LayoutUnit width = 0);
LayoutRect virtualRectForAreaElementAndDirection(HTMLAreaElement&, FocusType);
HTMLFrameOwnerElement* frameOwnerElement(FocusCandidate&);

} // namspace WebCore

#endif // SpatialNavigation_h
