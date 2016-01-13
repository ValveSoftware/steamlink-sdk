/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
 *
 */

#include "config.h"
#include "core/dom/DocumentMarkerController.h"

#include "core/dom/Node.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/Range.h"
#include "core/dom/RenderedDocumentMarker.h"
#include "core/editing/TextIterator.h"
#include "core/rendering/RenderObject.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace WebCore {

namespace {

DocumentMarker::MarkerTypeIndex MarkerTypeToMarkerIndex(DocumentMarker::MarkerType type)
{
    switch (type) {
    case DocumentMarker::Spelling:
        return DocumentMarker::SpellingMarkerIndex;
    case DocumentMarker::Grammar:
        return DocumentMarker::GramarMarkerIndex;
    case DocumentMarker::TextMatch:
        return DocumentMarker::TextMatchMarkerIndex;
    case DocumentMarker::InvisibleSpellcheck:
        return DocumentMarker::InvisibleSpellcheckMarkerIndex;
    }

    ASSERT_NOT_REACHED();
    return DocumentMarker::SpellingMarkerIndex;
}

} // namespace

inline bool DocumentMarkerController::possiblyHasMarkers(DocumentMarker::MarkerTypes types)
{
    return m_possiblyExistingMarkerTypes.intersects(types);
}

DocumentMarkerController::DocumentMarkerController()
    : m_possiblyExistingMarkerTypes(0)
{
}

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(DocumentMarkerController);

void DocumentMarkerController::clear()
{
    m_markers.clear();
    m_possiblyExistingMarkerTypes = 0;
}

void DocumentMarkerController::addMarker(Range* range, DocumentMarker::MarkerType type, const String& description, uint32_t hash)
{
    // Use a TextIterator to visit the potentially multiple nodes the range covers.
    for (TextIterator markedText(range); !markedText.atEnd(); markedText.advance()) {
        RefPtrWillBeRawPtr<Range> textPiece = markedText.range();
        addMarker(textPiece->startContainer(), DocumentMarker(type, textPiece->startOffset(), textPiece->endOffset(), description, hash));
    }
}

void DocumentMarkerController::addMarker(Range* range, DocumentMarker::MarkerType type, const String& description)
{
    // Use a TextIterator to visit the potentially multiple nodes the range covers.
    for (TextIterator markedText(range); !markedText.atEnd(); markedText.advance()) {
        RefPtrWillBeRawPtr<Range> textPiece = markedText.range();
        addMarker(textPiece->startContainer(), DocumentMarker(type, textPiece->startOffset(), textPiece->endOffset(), description));
    }
}

void DocumentMarkerController::addMarker(Range* range, DocumentMarker::MarkerType type)
{
    // Use a TextIterator to visit the potentially multiple nodes the range covers.
    for (TextIterator markedText(range); !markedText.atEnd(); markedText.advance()) {
        RefPtrWillBeRawPtr<Range> textPiece = markedText.range();
        addMarker(textPiece->startContainer(), DocumentMarker(type, textPiece->startOffset(), textPiece->endOffset()));
    }

}

void DocumentMarkerController::addTextMatchMarker(const Range* range, bool activeMatch)
{
    // Use a TextIterator to visit the potentially multiple nodes the range covers.
    for (TextIterator markedText(range); !markedText.atEnd(); markedText.advance()) {
        RefPtrWillBeRawPtr<Range> textPiece = markedText.range();
        unsigned startOffset = textPiece->startOffset();
        unsigned endOffset = textPiece->endOffset();
        addMarker(textPiece->startContainer(), DocumentMarker(startOffset, endOffset, activeMatch));
        if (endOffset > startOffset) {
            // Rendered rects for markers in WebKit are not populated until each time
            // the markers are painted. However, we need it to happen sooner, because
            // the whole purpose of tickmarks on the scrollbar is to show where
            // matches off-screen are (that haven't been painted yet).
            Node* node = textPiece->startContainer();
            WillBeHeapVector<DocumentMarker*> markers = markersFor(node);
            toRenderedDocumentMarker(markers[markers.size() - 1])->setRenderedRect(range->boundingBox());
        }
    }
}

void DocumentMarkerController::prepareForDestruction()
{
    clear();
}

void DocumentMarkerController::removeMarkers(Range* range, DocumentMarker::MarkerTypes markerTypes, RemovePartiallyOverlappingMarkerOrNot shouldRemovePartiallyOverlappingMarker)
{
    for (TextIterator markedText(range); !markedText.atEnd(); markedText.advance()) {
        if (!possiblyHasMarkers(markerTypes))
            return;
        ASSERT(!m_markers.isEmpty());

        RefPtrWillBeRawPtr<Range> textPiece = markedText.range();
        int startOffset = textPiece->startOffset();
        int endOffset = textPiece->endOffset();
        removeMarkers(textPiece->startContainer(), startOffset, endOffset - startOffset, markerTypes, shouldRemovePartiallyOverlappingMarker);
    }
}

static bool startsFurther(const DocumentMarker& lhv, const DocumentMarker& rhv)
{
    return lhv.startOffset() < rhv.startOffset();
}

static bool startsAfter(const DocumentMarker& marker, size_t startOffset)
{
    return marker.startOffset() < startOffset;
}

static bool endsBefore(size_t startOffset, const DocumentMarker& rhv)
{
    return startOffset < rhv.endOffset();
}

static bool compareByStart(const DocumentMarker* lhv, const DocumentMarker* rhv)
{
    return startsFurther(*lhv, *rhv);
}

static bool doesNotOverlap(const DocumentMarker& lhv, const DocumentMarker& rhv)
{
    return lhv.endOffset() < rhv.startOffset();
}

static bool doesNotInclude(const DocumentMarker& marker, size_t startOffset)
{
    return marker.endOffset() < startOffset;
}

// Markers are stored in order sorted by their start offset.
// Markers of the same type do not overlap each other.

void DocumentMarkerController::addMarker(Node* node, const DocumentMarker& newMarker)
{
    ASSERT(newMarker.endOffset() >= newMarker.startOffset());
    if (newMarker.endOffset() == newMarker.startOffset())
        return;

    m_possiblyExistingMarkerTypes.add(newMarker.type());

    OwnPtrWillBeMember<MarkerLists>& markers = m_markers.add(node, nullptr).storedValue->value;
    if (!markers) {
        markers = adoptPtrWillBeNoop(new MarkerLists);
        markers->grow(DocumentMarker::MarkerTypeIndexesCount);
    }

    DocumentMarker::MarkerTypeIndex markerListIndex = MarkerTypeToMarkerIndex(newMarker.type());
    if (!markers->at(markerListIndex)) {
        markers->insert(markerListIndex, adoptPtrWillBeNoop(new MarkerList));
    }

    OwnPtrWillBeMember<MarkerList>& list = markers->at(markerListIndex);
    if (list->isEmpty() || list->last().endOffset() < newMarker.startOffset()) {
        list->append(RenderedDocumentMarker(newMarker));
    } else {
        RenderedDocumentMarker toInsert(newMarker);
        if (toInsert.type() != DocumentMarker::TextMatch) {
            mergeOverlapping(list.get(), toInsert);
        } else {
            MarkerList::iterator pos = std::lower_bound(list->begin(), list->end(), toInsert, startsFurther);
            list->insert(pos - list->begin(), RenderedDocumentMarker(toInsert));
        }
    }

    // repaint the affected node
    if (node->renderer())
        node->renderer()->paintInvalidationForWholeRenderer();
}

void DocumentMarkerController::mergeOverlapping(MarkerList* list, DocumentMarker& toInsert)
{
    MarkerList::iterator firstOverlapping = std::lower_bound(list->begin(), list->end(), toInsert, doesNotOverlap);
    size_t index = firstOverlapping - list->begin();
    list->insert(index, RenderedDocumentMarker(toInsert));
    MarkerList::iterator inserted = list->begin() + index;
    firstOverlapping = inserted + 1;
    for (MarkerList::iterator i = firstOverlapping; i != list->end() && i->startOffset() <= inserted->endOffset(); ) {
        inserted->setStartOffset(std::min(inserted->startOffset(), i->startOffset()));
        inserted->setEndOffset(std::max(inserted->endOffset(), i->endOffset()));
        list->remove(i - list->begin());
    }
}

// copies markers from srcNode to dstNode, applying the specified shift delta to the copies.  The shift is
// useful if, e.g., the caller has created the dstNode from a non-prefix substring of the srcNode.
void DocumentMarkerController::copyMarkers(Node* srcNode, unsigned startOffset, int length, Node* dstNode, int delta)
{
    if (length <= 0)
        return;

    if (!possiblyHasMarkers(DocumentMarker::AllMarkers()))
        return;
    ASSERT(!m_markers.isEmpty());

    MarkerLists* markers = m_markers.get(srcNode);
    if (!markers)
        return;

    bool docDirty = false;
    for (size_t markerListIndex = 0; markerListIndex < DocumentMarker::MarkerTypeIndexesCount; ++markerListIndex) {
        OwnPtrWillBeMember<MarkerList>& list = (*markers)[markerListIndex];
        if (!list)
            continue;

        unsigned endOffset = startOffset + length - 1;
        MarkerList::iterator startPos = std::lower_bound(list->begin(), list->end(), startOffset, doesNotInclude);
        for (MarkerList::iterator i = startPos; i != list->end(); ++i) {
            DocumentMarker marker = *i;

            // stop if we are now past the specified range
            if (marker.startOffset() > endOffset)
                break;

            // pin the marker to the specified range and apply the shift delta
            docDirty = true;
            if (marker.startOffset() < startOffset)
                marker.setStartOffset(startOffset);
            if (marker.endOffset() > endOffset)
                marker.setEndOffset(endOffset);
            marker.shiftOffsets(delta);

            addMarker(dstNode, marker);
        }
    }

    // repaint the affected node
    if (docDirty && dstNode->renderer())
        dstNode->renderer()->paintInvalidationForWholeRenderer();
}

void DocumentMarkerController::removeMarkers(Node* node, unsigned startOffset, int length, DocumentMarker::MarkerTypes markerTypes, RemovePartiallyOverlappingMarkerOrNot shouldRemovePartiallyOverlappingMarker)
{
    if (length <= 0)
        return;

    if (!possiblyHasMarkers(markerTypes))
        return;
    ASSERT(!(m_markers.isEmpty()));

    MarkerLists* markers = m_markers.get(node);
    if (!markers)
        return;

    bool docDirty = false;
    size_t emptyListsCount = 0;
    for (size_t markerListIndex = 0; markerListIndex < DocumentMarker::MarkerTypeIndexesCount; ++markerListIndex) {
        OwnPtrWillBeMember<MarkerList>& list = (*markers)[markerListIndex];
        if (!list || list->isEmpty()) {
            if (list.get() && list->isEmpty())
                list.clear();
            ++emptyListsCount;
            continue;
        }
        if (!markerTypes.contains(list->begin()->type()))
            continue;
        unsigned endOffset = startOffset + length;
        MarkerList::iterator startPos = std::upper_bound(list->begin(), list->end(), startOffset, endsBefore);
        for (MarkerList::iterator i = startPos; i != list->end(); ) {
            DocumentMarker marker = *i;

            // markers are returned in order, so stop if we are now past the specified range
            if (marker.startOffset() >= endOffset)
                break;

            // at this point we know that marker and target intersect in some way
            docDirty = true;

            // pitch the old marker
            list->remove(i - list->begin());

            if (shouldRemovePartiallyOverlappingMarker) {
                // Stop here. Don't add resulting slices back.
                continue;
            }

            // add either of the resulting slices that are left after removing target
            if (startOffset > marker.startOffset()) {
                DocumentMarker newLeft = marker;
                newLeft.setEndOffset(startOffset);
                size_t insertIndex = i - list->begin();
                list->insert(insertIndex , RenderedDocumentMarker(newLeft));
                // Move to the marker after the inserted one.
                i = list->begin() + insertIndex + 1;
            }
            if (marker.endOffset() > endOffset) {
                DocumentMarker newRight = marker;
                newRight.setStartOffset(endOffset);
                size_t insertIndex = i - list->begin();
                list->insert(insertIndex, RenderedDocumentMarker(newRight));
                // Move to the marker after the inserted one.
                i = list->begin() + insertIndex + 1;
            }
        }

        if (list->isEmpty()) {
            list.clear();
            ++emptyListsCount;
        }
    }

    if (emptyListsCount == DocumentMarker::MarkerTypeIndexesCount) {
        m_markers.remove(node);
        if (m_markers.isEmpty())
            m_possiblyExistingMarkerTypes = 0;
    }

    // repaint the affected node
    if (docDirty && node->renderer())
        node->renderer()->paintInvalidationForWholeRenderer();
}

DocumentMarker* DocumentMarkerController::markerContainingPoint(const LayoutPoint& point, DocumentMarker::MarkerType markerType)
{
    if (!possiblyHasMarkers(markerType))
        return 0;
    ASSERT(!(m_markers.isEmpty()));

    // outer loop: process each node that contains any markers
    MarkerMap::iterator end = m_markers.end();
    for (MarkerMap::iterator nodeIterator = m_markers.begin(); nodeIterator != end; ++nodeIterator) {
        // inner loop; process each marker in this node
        MarkerLists* markers = nodeIterator->value.get();
        OwnPtrWillBeMember<MarkerList>& list = (*markers)[MarkerTypeToMarkerIndex(markerType)];
        unsigned markerCount = list.get() ? list->size() : 0;
        for (unsigned markerIndex = 0; markerIndex < markerCount; ++markerIndex) {
            RenderedDocumentMarker& marker = list->at(markerIndex);

            if (marker.contains(point))
                return &marker;
        }
    }

    return 0;
}

WillBeHeapVector<DocumentMarker*> DocumentMarkerController::markersFor(Node* node, DocumentMarker::MarkerTypes markerTypes)
{
    WillBeHeapVector<DocumentMarker*> result;

    MarkerLists* markers = m_markers.get(node);
    if (!markers)
        return result;

    for (size_t markerListIndex = 0; markerListIndex < DocumentMarker::MarkerTypeIndexesCount; ++markerListIndex) {
        OwnPtrWillBeMember<MarkerList>& list = (*markers)[markerListIndex];
        if (!list || list->isEmpty() || !markerTypes.contains(list->begin()->type()))
            continue;

        for (size_t i = 0; i < list->size(); ++i)
            result.append(&(list->at(i)));
    }

    std::sort(result.begin(), result.end(), compareByStart);
    return result;
}

WillBeHeapVector<DocumentMarker*> DocumentMarkerController::markers()
{
    WillBeHeapVector<DocumentMarker*> result;
    for (MarkerMap::iterator i = m_markers.begin(); i != m_markers.end(); ++i) {
        MarkerLists* markers = i->value.get();
        for (size_t markerListIndex = 0; markerListIndex < DocumentMarker::MarkerTypeIndexesCount; ++markerListIndex) {
            OwnPtrWillBeMember<MarkerList>& list = (*markers)[markerListIndex];
            for (size_t j = 0; list.get() && j < list->size(); ++j)
                result.append(&(list->at(j)));
        }
    }
    std::sort(result.begin(), result.end(), compareByStart);
    return result;
}

WillBeHeapVector<DocumentMarker*> DocumentMarkerController::markersInRange(Range* range, DocumentMarker::MarkerTypes markerTypes)
{
    if (!possiblyHasMarkers(markerTypes))
        return WillBeHeapVector<DocumentMarker*>();

    WillBeHeapVector<DocumentMarker*> foundMarkers;

    Node* startContainer = range->startContainer();
    ASSERT(startContainer);
    Node* endContainer = range->endContainer();
    ASSERT(endContainer);

    Node* pastLastNode = range->pastLastNode();
    for (Node* node = range->firstNode(); node != pastLastNode; node = NodeTraversal::next(*node)) {
        WillBeHeapVector<DocumentMarker*> markers = markersFor(node);
        WillBeHeapVector<DocumentMarker*>::const_iterator end = markers.end();
        for (WillBeHeapVector<DocumentMarker*>::const_iterator it = markers.begin(); it != end; ++it) {
            DocumentMarker* marker = *it;
            if (!markerTypes.contains(marker->type()))
                continue;
            if (node == startContainer && marker->endOffset() <= static_cast<unsigned>(range->startOffset()))
                continue;
            if (node == endContainer && marker->startOffset() >= static_cast<unsigned>(range->endOffset()))
                continue;
            foundMarkers.append(marker);
        }
    }
    return foundMarkers;
}

Vector<IntRect> DocumentMarkerController::renderedRectsForMarkers(DocumentMarker::MarkerType markerType)
{
    Vector<IntRect> result;

    if (!possiblyHasMarkers(markerType))
        return result;
    ASSERT(!(m_markers.isEmpty()));

    // outer loop: process each node
    MarkerMap::iterator end = m_markers.end();
    for (MarkerMap::iterator nodeIterator = m_markers.begin(); nodeIterator != end; ++nodeIterator) {
        // inner loop; process each marker in this node
        MarkerLists* markers = nodeIterator->value.get();
        for (size_t markerListIndex = 0; markerListIndex < DocumentMarker::MarkerTypeIndexesCount; ++markerListIndex) {
            OwnPtrWillBeMember<MarkerList>& list = (*markers)[markerListIndex];
            if (!list || list->isEmpty() || list->begin()->type() != markerType)
                continue;
            for (unsigned markerIndex = 0; markerIndex < list->size(); ++markerIndex) {
                const RenderedDocumentMarker& marker = list->at(markerIndex);

                if (!marker.isRendered())
                    continue;

                result.append(marker.renderedRect());
            }
        }
    }

    return result;
}

void DocumentMarkerController::trace(Visitor* visitor)
{
    visitor->trace(m_markers);
}

void DocumentMarkerController::removeMarkers(Node* node, DocumentMarker::MarkerTypes markerTypes)
{
    if (!possiblyHasMarkers(markerTypes))
        return;
    ASSERT(!m_markers.isEmpty());

    MarkerMap::iterator iterator = m_markers.find(node);
    if (iterator != m_markers.end())
        removeMarkersFromList(iterator, markerTypes);
}

void DocumentMarkerController::removeMarkers(DocumentMarker::MarkerTypes markerTypes)
{
    if (!possiblyHasMarkers(markerTypes))
        return;
    ASSERT(!m_markers.isEmpty());

    Vector<const Node*> nodesWithMarkers;
    copyKeysToVector(m_markers, nodesWithMarkers);
    unsigned size = nodesWithMarkers.size();
    for (unsigned i = 0; i < size; ++i) {
        MarkerMap::iterator iterator = m_markers.find(nodesWithMarkers[i]);
        if (iterator != m_markers.end())
            removeMarkersFromList(iterator, markerTypes);
    }

    m_possiblyExistingMarkerTypes.remove(markerTypes);
}

void DocumentMarkerController::removeMarkersFromList(MarkerMap::iterator iterator, DocumentMarker::MarkerTypes markerTypes)
{
    bool needsRepainting = false;
    bool nodeCanBeRemoved;

    size_t emptyListsCount = 0;
    if (markerTypes == DocumentMarker::AllMarkers()) {
        needsRepainting = true;
        nodeCanBeRemoved = true;
    } else {
        MarkerLists* markers = iterator->value.get();

        for (size_t markerListIndex = 0; markerListIndex < DocumentMarker::MarkerTypeIndexesCount; ++markerListIndex) {
            OwnPtrWillBeMember<MarkerList>& list = (*markers)[markerListIndex];
            if (!list || list->isEmpty()) {
                if (list.get() && list->isEmpty())
                    list.clear();
                ++emptyListsCount;
                continue;
            }
            if (markerTypes.contains(list->begin()->type())) {
                list->clear();
                list.clear();
                ++emptyListsCount;
                needsRepainting = true;
            }
        }

        nodeCanBeRemoved = emptyListsCount == DocumentMarker::MarkerTypeIndexesCount;
    }

    if (needsRepainting) {
        if (RenderObject* renderer = iterator->key->renderer())
            renderer->paintInvalidationForWholeRenderer();
    }

    if (nodeCanBeRemoved) {
        m_markers.remove(iterator);
        if (m_markers.isEmpty())
            m_possiblyExistingMarkerTypes = 0;
    }
}

void DocumentMarkerController::repaintMarkers(DocumentMarker::MarkerTypes markerTypes)
{
    if (!possiblyHasMarkers(markerTypes))
        return;
    ASSERT(!m_markers.isEmpty());

    // outer loop: process each markered node in the document
    MarkerMap::iterator end = m_markers.end();
    for (MarkerMap::iterator i = m_markers.begin(); i != end; ++i) {
        const Node* node = i->key;

        // inner loop: process each marker in the current node
        MarkerLists* markers = i->value.get();
        for (size_t markerListIndex = 0; markerListIndex < DocumentMarker::MarkerTypeIndexesCount; ++markerListIndex) {
            OwnPtrWillBeMember<MarkerList>& list = (*markers)[markerListIndex];
            if (!list || list->isEmpty() || !markerTypes.contains(list->begin()->type()))
                continue;

            // cause the node to be redrawn
            if (RenderObject* renderer = node->renderer()) {
                renderer->paintInvalidationForWholeRenderer();
                break;
            }
        }
    }
}

void DocumentMarkerController::invalidateRenderedRectsForMarkersInRect(const LayoutRect& r)
{
    // outer loop: process each markered node in the document
    MarkerMap::iterator end = m_markers.end();
    for (MarkerMap::iterator i = m_markers.begin(); i != end; ++i) {

        // inner loop: process each rect in the current node
        MarkerLists* markers = i->value.get();
        for (size_t markerListIndex = 0; markerListIndex < DocumentMarker::MarkerTypeIndexesCount; ++markerListIndex) {
            OwnPtrWillBeMember<MarkerList>& list = (*markers)[markerListIndex];
            for (size_t markerIndex = 0; list.get() && markerIndex < list->size(); ++markerIndex)
                list->at(markerIndex).invalidate(r);
        }
    }
}

void DocumentMarkerController::shiftMarkers(Node* node, unsigned startOffset, int delta)
{
    if (!possiblyHasMarkers(DocumentMarker::AllMarkers()))
        return;
    ASSERT(!m_markers.isEmpty());

    MarkerLists* markers = m_markers.get(node);
    if (!markers)
        return;

    bool docDirty = false;
    for (size_t markerListIndex = 0; markerListIndex < DocumentMarker::MarkerTypeIndexesCount; ++markerListIndex) {
        OwnPtrWillBeMember<MarkerList>& list = (*markers)[markerListIndex];
        if (!list)
            continue;
        MarkerList::iterator startPos = std::lower_bound(list->begin(), list->end(), startOffset, startsAfter);
        for (MarkerList::iterator marker = startPos; marker != list->end(); ++marker) {
            ASSERT((int)marker->startOffset() + delta >= 0);
            marker->shiftOffsets(delta);
            docDirty = true;

            // Marker moved, so previously-computed rendered rectangle is now invalid
            marker->invalidate();
        }
    }

    // repaint the affected node
    if (docDirty && node->renderer())
        node->renderer()->paintInvalidationForWholeRenderer();
}

void DocumentMarkerController::setMarkersActive(Range* range, bool active)
{
    if (!possiblyHasMarkers(DocumentMarker::AllMarkers()))
        return;
    ASSERT(!m_markers.isEmpty());

    Node* startContainer = range->startContainer();
    Node* endContainer = range->endContainer();

    Node* pastLastNode = range->pastLastNode();

    for (Node* node = range->firstNode(); node != pastLastNode; node = NodeTraversal::next(*node)) {
        int startOffset = node == startContainer ? range->startOffset() : 0;
        int endOffset = node == endContainer ? range->endOffset() : INT_MAX;
        setMarkersActive(node, startOffset, endOffset, active);
    }
}

void DocumentMarkerController::setMarkersActive(Node* node, unsigned startOffset, unsigned endOffset, bool active)
{
    MarkerLists* markers = m_markers.get(node);
    if (!markers)
        return;

    bool docDirty = false;
    OwnPtrWillBeMember<MarkerList>& list = (*markers)[MarkerTypeToMarkerIndex(DocumentMarker::TextMatch)];
    if (!list)
        return;
    MarkerList::iterator startPos = std::upper_bound(list->begin(), list->end(), startOffset, endsBefore);
    for (MarkerList::iterator marker = startPos; marker != list->end(); ++marker) {

        // Markers are returned in order, so stop if we are now past the specified range.
        if (marker->startOffset() >= endOffset)
            break;

        marker->setActiveMatch(active);
        docDirty = true;
    }

    // repaint the affected node
    if (docDirty && node->renderer())
        node->renderer()->paintInvalidationForWholeRenderer();
}

bool DocumentMarkerController::hasMarkers(Range* range, DocumentMarker::MarkerTypes markerTypes)
{
    if (!possiblyHasMarkers(markerTypes))
        return false;
    ASSERT(!m_markers.isEmpty());

    Node* startContainer = range->startContainer();
    ASSERT(startContainer);
    Node* endContainer = range->endContainer();
    ASSERT(endContainer);

    Node* pastLastNode = range->pastLastNode();
    for (Node* node = range->firstNode(); node != pastLastNode; node = NodeTraversal::next(*node)) {
        WillBeHeapVector<DocumentMarker*> markers = markersFor(node);
        WillBeHeapVector<DocumentMarker*>::const_iterator end = markers.end();
        for (WillBeHeapVector<DocumentMarker*>::const_iterator it = markers.begin(); it != end; ++it) {
            DocumentMarker* marker = *it;
            if (!markerTypes.contains(marker->type()))
                continue;
            if (node == startContainer && marker->endOffset() <= static_cast<unsigned>(range->startOffset()))
                continue;
            if (node == endContainer && marker->startOffset() >= static_cast<unsigned>(range->endOffset()))
                continue;
            return true;
        }
    }
    return false;
}

#ifndef NDEBUG
void DocumentMarkerController::showMarkers() const
{
    fprintf(stderr, "%d nodes have markers:\n", m_markers.size());
    MarkerMap::const_iterator end = m_markers.end();
    for (MarkerMap::const_iterator nodeIterator = m_markers.begin(); nodeIterator != end; ++nodeIterator) {
        const Node* node = nodeIterator->key;
        fprintf(stderr, "%p", node);
        MarkerLists* markers = m_markers.get(node);
        for (size_t markerListIndex = 0; markerListIndex < DocumentMarker::MarkerTypeIndexesCount; ++markerListIndex) {
            OwnPtrWillBeMember<MarkerList>& list = (*markers)[markerListIndex];
            for (unsigned markerIndex = 0; list.get() && markerIndex < list->size(); ++markerIndex) {
                const DocumentMarker& marker = list->at(markerIndex);
                fprintf(stderr, " %d:[%d:%d](%d)", marker.type(), marker.startOffset(), marker.endOffset(), marker.activeMatch());
            }
        }

        fprintf(stderr, "\n");
    }
}
#endif

} // namespace WebCore

#ifndef NDEBUG
void showDocumentMarkers(const WebCore::DocumentMarkerController* controller)
{
    if (controller)
        controller->showMarkers();
}
#endif
