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

#include "core/editing/markers/DocumentMarkerController.h"

#include "core/dom/Node.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/markers/RenderedDocumentMarker.h"
#include "core/frame/FrameView.h"
#include "core/layout/LayoutObject.h"
#include <algorithm>

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace blink {

MarkerRemoverPredicate::MarkerRemoverPredicate(const Vector<String>& words)
    : m_words(words)
{
}

bool MarkerRemoverPredicate::operator()(const DocumentMarker& documentMarker, const Text& textNode) const
{
    unsigned start  = documentMarker.startOffset();
    unsigned length = documentMarker.endOffset() - documentMarker.startOffset();

    String markerText = textNode.data().substring(start, length);
    return m_words.contains(markerText);
}

namespace {

DocumentMarker::MarkerTypeIndex MarkerTypeToMarkerIndex(DocumentMarker::MarkerType type)
{
    switch (type) {
    case DocumentMarker::Spelling:
        return DocumentMarker::SpellingMarkerIndex;
    case DocumentMarker::Grammar:
        return DocumentMarker::GrammarMarkerIndex;
    case DocumentMarker::TextMatch:
        return DocumentMarker::TextMatchMarkerIndex;
    case DocumentMarker::InvisibleSpellcheck:
        return DocumentMarker::InvisibleSpellcheckMarkerIndex;
    case DocumentMarker::Composition:
        return DocumentMarker::CompositionMarkerIndex;
    }

    NOTREACHED();
    return DocumentMarker::SpellingMarkerIndex;
}

} // namespace

inline bool DocumentMarkerController::possiblyHasMarkers(DocumentMarker::MarkerTypes types)
{
    return m_possiblyExistingMarkerTypes.intersects(types);
}

DocumentMarkerController::DocumentMarkerController(const Document& document)
    : m_possiblyExistingMarkerTypes(0)
    , m_document(&document)
{
}

void DocumentMarkerController::clear()
{
    m_markers.clear();
    m_possiblyExistingMarkerTypes = 0;
}

void DocumentMarkerController::addMarker(const Position& start, const Position& end, DocumentMarker::MarkerType type, const String& description, uint32_t hash)
{
    // TODO(dglazkov): The use of updateStyleAndLayoutIgnorePendingStylesheets needs to be audited.
    // see http://crbug.com/590369 for more details.
    start.document()->updateStyleAndLayoutIgnorePendingStylesheets();

    // Use a TextIterator to visit the potentially multiple nodes the range covers.
    for (TextIterator markedText(start, end); !markedText.atEnd(); markedText.advance()) {
        addMarker(markedText.currentContainer(), DocumentMarker(type, markedText.startOffsetInCurrentContainer(), markedText.endOffsetInCurrentContainer(), description, hash));
    }
}

void DocumentMarkerController::addTextMatchMarker(const Range* range, bool activeMatch)
{
    // TODO(dglazkov): The use of updateStyleAndLayoutIgnorePendingStylesheets needs to be audited.
    // see http://crbug.com/590369 for more details.
    range->startPosition().document()->updateStyleAndLayoutIgnorePendingStylesheets();

    // Use a TextIterator to visit the potentially multiple nodes the range covers.
    for (TextIterator markedText(range->startPosition(), range->endPosition()); !markedText.atEnd(); markedText.advance())
        addMarker(markedText.currentContainer(), DocumentMarker(markedText.startOffsetInCurrentContainer(), markedText.endOffsetInCurrentContainer(), activeMatch));
    // Don't invalidate tickmarks here. TextFinder invalidates tickmarks using a throttling algorithm. crbug.com/6819.
}

void DocumentMarkerController::addCompositionMarker(const Position& start, const Position& end, Color underlineColor, bool thick, Color backgroundColor)
{
    // TODO(dglazkov): The use of updateStyleAndLayoutIgnorePendingStylesheets needs to be audited.
    // see http://crbug.com/590369 for more details.
    start.document()->updateStyleAndLayoutIgnorePendingStylesheets();

    for (TextIterator markedText(start, end); !markedText.atEnd(); markedText.advance())
        addMarker(markedText.currentContainer(), DocumentMarker(markedText.startOffsetInCurrentContainer(), markedText.endOffsetInCurrentContainer(), underlineColor, thick, backgroundColor));
}

void DocumentMarkerController::prepareForDestruction()
{
    clear();
}

void DocumentMarkerController::removeMarkers(TextIterator& markedText, DocumentMarker::MarkerTypes markerTypes, RemovePartiallyOverlappingMarkerOrNot shouldRemovePartiallyOverlappingMarker)
{
    for (; !markedText.atEnd(); markedText.advance()) {
        if (!possiblyHasMarkers(markerTypes))
            return;
        DCHECK(!m_markers.isEmpty());

        int startOffset = markedText.startOffsetInCurrentContainer();
        int endOffset = markedText.endOffsetInCurrentContainer();
        removeMarkers(markedText.currentContainer(), startOffset, endOffset - startOffset, markerTypes, shouldRemovePartiallyOverlappingMarker);
    }
}

void DocumentMarkerController::removeMarkers(const EphemeralRange& range, DocumentMarker::MarkerTypes markerTypes, RemovePartiallyOverlappingMarkerOrNot shouldRemovePartiallyOverlappingMarker)
{
    // TODO(dglazkov): The use of updateStyleAndLayoutIgnorePendingStylesheets needs to be audited.
    // see http://crbug.com/590369 for more details.
    range.startPosition().document()->updateStyleAndLayoutIgnorePendingStylesheets();

    TextIterator markedText(range.startPosition(), range.endPosition());
    DocumentMarkerController::removeMarkers(markedText, markerTypes, shouldRemovePartiallyOverlappingMarker);
}

static bool startsFurther(const Member<RenderedDocumentMarker>& lhv, const DocumentMarker* rhv)
{
    return lhv->startOffset() < rhv->startOffset();
}

static bool startsAfter(const Member<RenderedDocumentMarker>& marker, size_t startOffset)
{
    return marker->startOffset() < startOffset;
}

static bool endsBefore(size_t startOffset, const Member<RenderedDocumentMarker>& rhv)
{
    return startOffset < rhv->endOffset();
}

static bool compareByStart(const Member<DocumentMarker>& lhv, const Member<DocumentMarker>& rhv)
{
    return lhv->startOffset() < rhv->startOffset();
}

static bool doesNotOverlap(const Member<RenderedDocumentMarker>& lhv, const DocumentMarker* rhv)
{
    return lhv->endOffset() < rhv->startOffset();
}

static bool doesNotInclude(const Member<RenderedDocumentMarker>& marker, size_t startOffset)
{
    return marker->endOffset() < startOffset;
}

static bool updateMarkerRenderedRect(const Node& node, RenderedDocumentMarker& marker)
{
    Range* range = Range::create(node.document());
    // The offsets of the marker may be out-dated, so check for exceptions.
    TrackExceptionState exceptionState;
    range->setStart(&const_cast<Node&>(node), marker.startOffset(), exceptionState);
    if (!exceptionState.hadException())
        range->setEnd(&const_cast<Node&>(node), marker.endOffset(), IGNORE_EXCEPTION);
    if (exceptionState.hadException()) {
        range->dispose();
        return marker.nullifyRenderedRect();
    }
    // TODO(yosin): Once we have a |EphemeralRange| version of |boundingBox()|,
    // we should use it instead of |Range| version.
    const bool isUpdated = marker.setRenderedRect(LayoutRect(range->boundingBox()));
    range->dispose();
    return isUpdated;
}

// Markers are stored in order sorted by their start offset.
// Markers of the same type do not overlap each other.

void DocumentMarkerController::addMarker(Node* node, const DocumentMarker& newMarker)
{
    DCHECK_GE(newMarker.endOffset(), newMarker.startOffset());
    if (newMarker.endOffset() == newMarker.startOffset())
        return;

    m_possiblyExistingMarkerTypes.add(newMarker.type());

    Member<MarkerLists>& markers = m_markers.add(node, nullptr).storedValue->value;
    if (!markers) {
        markers = new MarkerLists;
        markers->grow(DocumentMarker::MarkerTypeIndexesCount);
    }

    DocumentMarker::MarkerTypeIndex markerListIndex = MarkerTypeToMarkerIndex(newMarker.type());
    if (!markers->at(markerListIndex)) {
        markers->insert(markerListIndex, new MarkerList);
    }

    Member<MarkerList>& list = markers->at(markerListIndex);
    RenderedDocumentMarker* newRenderedMarker = RenderedDocumentMarker::create(newMarker);
    updateMarkerRenderedRect(*node, *newRenderedMarker);
    if (list->isEmpty() || list->last()->endOffset() < newMarker.startOffset()) {
        list->append(newRenderedMarker);
    } else {
        if (newMarker.type() != DocumentMarker::TextMatch && newMarker.type() != DocumentMarker::Composition) {
            mergeOverlapping(list.get(), newRenderedMarker);
        } else {
            MarkerList::iterator pos = std::lower_bound(list->begin(), list->end(), &newMarker, startsFurther);
            list->insert(pos - list->begin(), newRenderedMarker);
        }
    }

    // repaint the affected node
    if (node->layoutObject())
        node->layoutObject()->setShouldDoFullPaintInvalidation();
}

void DocumentMarkerController::mergeOverlapping(MarkerList* list, RenderedDocumentMarker* toInsert)
{
    MarkerList::iterator firstOverlapping = std::lower_bound(list->begin(), list->end(), toInsert, doesNotOverlap);
    size_t index = firstOverlapping - list->begin();
    list->insert(index, toInsert);
    MarkerList::iterator inserted = list->begin() + index;
    firstOverlapping = inserted + 1;
    for (MarkerList::iterator i = firstOverlapping; i != list->end() && (*i)->startOffset() <= (*inserted)->endOffset(); ) {
        (*inserted)->setStartOffset(std::min((*inserted)->startOffset(), (*i)->startOffset()));
        (*inserted)->setEndOffset(std::max((*inserted)->endOffset(), (*i)->endOffset()));
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
    DCHECK(!m_markers.isEmpty());

    MarkerLists* markers = m_markers.get(srcNode);
    if (!markers)
        return;

    bool docDirty = false;
    for (size_t markerListIndex = 0; markerListIndex < DocumentMarker::MarkerTypeIndexesCount; ++markerListIndex) {
        Member<MarkerList>& list = (*markers)[markerListIndex];
        if (!list)
            continue;

        unsigned endOffset = startOffset + length - 1;
        MarkerList::iterator startPos = std::lower_bound(list->begin(), list->end(), startOffset, doesNotInclude);
        for (MarkerList::iterator i = startPos; i != list->end(); ++i) {
            DocumentMarker* marker = i->get();

            // stop if we are now past the specified range
            if (marker->startOffset() > endOffset)
                break;

            // pin the marker to the specified range and apply the shift delta
            docDirty = true;
            if (marker->startOffset() < startOffset)
                marker->setStartOffset(startOffset);
            if (marker->endOffset() > endOffset)
                marker->setEndOffset(endOffset);
            marker->shiftOffsets(delta);

            addMarker(dstNode, *marker);
        }
    }

    // repaint the affected node
    if (docDirty && dstNode->layoutObject())
        dstNode->layoutObject()->setShouldDoFullPaintInvalidation();
}

void DocumentMarkerController::removeMarkers(Node* node, unsigned startOffset, int length, DocumentMarker::MarkerTypes markerTypes, RemovePartiallyOverlappingMarkerOrNot shouldRemovePartiallyOverlappingMarker)
{
    if (length <= 0)
        return;

    if (!possiblyHasMarkers(markerTypes))
        return;
    DCHECK(!(m_markers.isEmpty()));

    MarkerLists* markers = m_markers.get(node);
    if (!markers)
        return;

    bool docDirty = false;
    size_t emptyListsCount = 0;
    for (size_t markerListIndex = 0; markerListIndex < DocumentMarker::MarkerTypeIndexesCount; ++markerListIndex) {
        Member<MarkerList>& list = (*markers)[markerListIndex];
        if (!list || list->isEmpty()) {
            if (list.get() && list->isEmpty())
                list.clear();
            ++emptyListsCount;
            continue;
        }
        if (!markerTypes.contains((*list->begin())->type()))
            continue;
        unsigned endOffset = startOffset + length;
        MarkerList::iterator startPos = std::upper_bound(list->begin(), list->end(), startOffset, endsBefore);
        for (MarkerList::iterator i = startPos; i != list->end(); ) {
            DocumentMarker marker(*i->get());

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
                list->insert(insertIndex, RenderedDocumentMarker::create(newLeft));
                // Move to the marker after the inserted one.
                i = list->begin() + insertIndex + 1;
            }
            if (marker.endOffset() > endOffset) {
                DocumentMarker newRight = marker;
                newRight.setStartOffset(endOffset);
                size_t insertIndex = i - list->begin();
                list->insert(insertIndex, RenderedDocumentMarker::create(newRight));
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
    if (docDirty && node->layoutObject())
        node->layoutObject()->setShouldDoFullPaintInvalidation();
}

DocumentMarker* DocumentMarkerController::markerContainingPoint(const LayoutPoint& point, DocumentMarker::MarkerType markerType)
{
    if (!possiblyHasMarkers(markerType))
        return 0;
    DCHECK(!(m_markers.isEmpty()));

    // outer loop: process each node that contains any markers
    for (auto& nodeMarkers : m_markers) {
        const Node& node = *nodeMarkers.key;
        // inner loop; process each marker in this node
        Member<MarkerList>& markerList = (*nodeMarkers.value)[MarkerTypeToMarkerIndex(markerType)];
        for (auto& marker : *markerList) {
            updateMarkerRenderedRectIfNeeded(node, *marker);
            if (marker->contains(point))
                return marker;
        }
    }

    return 0;
}

DocumentMarkerVector DocumentMarkerController::markersFor(Node* node, DocumentMarker::MarkerTypes markerTypes)
{
    DocumentMarkerVector result;

    MarkerLists* markers = m_markers.get(node);
    if (!markers)
        return result;

    for (size_t markerListIndex = 0; markerListIndex < DocumentMarker::MarkerTypeIndexesCount; ++markerListIndex) {
        Member<MarkerList>& list = (*markers)[markerListIndex];
        if (!list || list->isEmpty() || !markerTypes.contains((*list->begin())->type()))
            continue;

        for (size_t i = 0; i < list->size(); ++i)
            result.append(list->at(i).get());
    }

    std::sort(result.begin(), result.end(), compareByStart);
    return result;
}

DocumentMarkerVector DocumentMarkerController::markers()
{
    DocumentMarkerVector result;
    for (MarkerMap::iterator i = m_markers.begin(); i != m_markers.end(); ++i) {
        MarkerLists* markers = i->value.get();
        for (size_t markerListIndex = 0; markerListIndex < DocumentMarker::MarkerTypeIndexesCount; ++markerListIndex) {
            Member<MarkerList>& list = (*markers)[markerListIndex];
            for (size_t j = 0; list.get() && j < list->size(); ++j)
                result.append(list->at(j).get());
        }
    }
    std::sort(result.begin(), result.end(), compareByStart);
    return result;
}

DocumentMarkerVector DocumentMarkerController::markersInRange(const EphemeralRange& range, DocumentMarker::MarkerTypes markerTypes)
{
    if (!possiblyHasMarkers(markerTypes))
        return DocumentMarkerVector();

    DocumentMarkerVector foundMarkers;

    Node* startContainer = range.startPosition().computeContainerNode();
    DCHECK(startContainer);
    unsigned startOffset = static_cast<unsigned>(range.startPosition().computeOffsetInContainerNode());
    Node* endContainer = range.endPosition().computeContainerNode();
    DCHECK(endContainer);
    unsigned endOffset = static_cast<unsigned>(range.endPosition().computeOffsetInContainerNode());

    Node* pastLastNode = range.endPosition().nodeAsRangePastLastNode();
    for (Node* node = range.startPosition().nodeAsRangeFirstNode(); node != pastLastNode; node = NodeTraversal::next(*node)) {
        for (DocumentMarker* marker : markersFor(node)) {
            if (!markerTypes.contains(marker->type()))
                continue;
            if (node == startContainer && marker->endOffset() <= startOffset)
                continue;
            if (node == endContainer && marker->startOffset() >= endOffset)
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
    DCHECK(!(m_markers.isEmpty()));

    // outer loop: process each node
    MarkerMap::iterator end = m_markers.end();
    for (MarkerMap::iterator nodeIterator = m_markers.begin(); nodeIterator != end; ++nodeIterator) {
        // inner loop; process each marker in this node
        const Node& node = *nodeIterator->key;
        MarkerLists* markers = nodeIterator->value.get();
        for (size_t markerListIndex = 0; markerListIndex < DocumentMarker::MarkerTypeIndexesCount; ++markerListIndex) {
            Member<MarkerList>& list = (*markers)[markerListIndex];
            if (!list || list->isEmpty() || (*list->begin())->type() != markerType)
                continue;
            for (unsigned markerIndex = 0; markerIndex < list->size(); ++markerIndex) {
                RenderedDocumentMarker* marker = list->at(markerIndex).get();
                updateMarkerRenderedRectIfNeeded(node, *marker);
                if (!marker->isRendered())
                    continue;
                result.append(marker->renderedRect());
            }
        }
    }

    return result;
}

static void invalidatePaintForTickmarks(const Node& node)
{
    if (FrameView* frameView = node.document().view())
        frameView->invalidatePaintForTickmarks();
}

void DocumentMarkerController::updateMarkerRenderedRectIfNeeded(const Node& node, RenderedDocumentMarker& marker)
{
    DCHECK(!m_document->view() || !m_document->view()->needsLayout());
    DCHECK(!m_document->needsLayoutTreeUpdate());
    if (!marker.isValid())
        updateMarkerRenderedRect(node, marker);
}

void DocumentMarkerController::invalidateRectsForMarkersInNode(const Node& node)
{
    MarkerLists* markers = m_markers.get(&node);

    for (auto& markerList : *markers) {
        if (!markerList || markerList->isEmpty())
            continue;

        for (auto& marker : *markerList)
            marker->invalidate();

        if (markerList->first()->type() == DocumentMarker::TextMatch)
            invalidatePaintForTickmarks(node);
    }
}

void DocumentMarkerController::invalidateRectsForAllMarkers()
{
    for (auto& nodeMarkers : m_markers) {
        const Node& node = *nodeMarkers.key;
        for (auto& markerList : *nodeMarkers.value) {
            if (!markerList || markerList->isEmpty())
                continue;

            for (auto& marker : *markerList)
                marker->invalidate();

            if (markerList->first()->type() == DocumentMarker::TextMatch)
                invalidatePaintForTickmarks(node);
        }
    }
}

DEFINE_TRACE(DocumentMarkerController)
{
    visitor->trace(m_markers);
    visitor->trace(m_document);
}

void DocumentMarkerController::removeMarkers(Node* node, DocumentMarker::MarkerTypes markerTypes)
{
    if (!possiblyHasMarkers(markerTypes))
        return;
    DCHECK(!m_markers.isEmpty());

    MarkerMap::iterator iterator = m_markers.find(node);
    if (iterator != m_markers.end())
        removeMarkersFromList(iterator, markerTypes);
}

void DocumentMarkerController::removeMarkers(const MarkerRemoverPredicate& shouldRemoveMarker)
{
    for (auto& nodeMarkers : m_markers) {
        const Node& node = *nodeMarkers.key;
        if (!node.isTextNode()) // MarkerRemoverPredicate requires a Text node.
            continue;
        MarkerLists& markers = *nodeMarkers.value;
        for (size_t markerListIndex = 0; markerListIndex < DocumentMarker::MarkerTypeIndexesCount; ++markerListIndex) {
            Member<MarkerList>& list = markers[markerListIndex];
            if (!list)
                continue;
            bool removedMarkers = false;
            for (size_t j = list->size(); j > 0; --j) {
                if (shouldRemoveMarker(*list->at(j - 1), static_cast<const Text&>(node))) {
                    list->remove(j - 1);
                    removedMarkers = true;
                }
            }
            if (removedMarkers && markerListIndex == DocumentMarker::TextMatchMarkerIndex)
                invalidatePaintForTickmarks(node);
        }
    }
}

void DocumentMarkerController::removeMarkers(DocumentMarker::MarkerTypes markerTypes)
{
    if (!possiblyHasMarkers(markerTypes))
        return;
    DCHECK(!m_markers.isEmpty());

    HeapVector<Member<const Node>> nodesWithMarkers;
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
            Member<MarkerList>& list = (*markers)[markerListIndex];
            if (!list || list->isEmpty()) {
                if (list.get() && list->isEmpty())
                    list.clear();
                ++emptyListsCount;
                continue;
            }
            if (markerTypes.contains((*list->begin())->type())) {
                list->clear();
                list.clear();
                ++emptyListsCount;
                needsRepainting = true;
            }
        }

        nodeCanBeRemoved = emptyListsCount == DocumentMarker::MarkerTypeIndexesCount;
    }

    if (needsRepainting) {
        const Node& node = *iterator->key;
        if (LayoutObject* layoutObject = node.layoutObject())
            layoutObject->setShouldDoFullPaintInvalidation();
        invalidatePaintForTickmarks(node);
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
    DCHECK(!m_markers.isEmpty());

    // outer loop: process each markered node in the document
    MarkerMap::iterator end = m_markers.end();
    for (MarkerMap::iterator i = m_markers.begin(); i != end; ++i) {
        const Node* node = i->key;

        // inner loop: process each marker in the current node
        MarkerLists* markers = i->value.get();
        for (size_t markerListIndex = 0; markerListIndex < DocumentMarker::MarkerTypeIndexesCount; ++markerListIndex) {
            Member<MarkerList>& list = (*markers)[markerListIndex];
            if (!list || list->isEmpty() || !markerTypes.contains((*list->begin())->type()))
                continue;

            // cause the node to be redrawn
            if (LayoutObject* layoutObject = node->layoutObject()) {
                layoutObject->setShouldDoFullPaintInvalidation();
                break;
            }
        }
    }
}

void DocumentMarkerController::shiftMarkers(Node* node, unsigned startOffset, int delta)
{
    if (!possiblyHasMarkers(DocumentMarker::AllMarkers()))
        return;
    DCHECK(!m_markers.isEmpty());

    MarkerLists* markers = m_markers.get(node);
    if (!markers)
        return;

    bool didShiftMarker = false;
    for (size_t markerListIndex = 0; markerListIndex < DocumentMarker::MarkerTypeIndexesCount; ++markerListIndex) {
        Member<MarkerList>& list = (*markers)[markerListIndex];
        if (!list)
            continue;
        MarkerList::iterator startPos = std::lower_bound(list->begin(), list->end(), startOffset, startsAfter);
        for (MarkerList::iterator marker = startPos; marker != list->end(); ++marker) {
#if DCHECK_IS_ON()
            int startOffset = (*marker)->startOffset();
            DCHECK_GE(startOffset + delta, 0);
#endif
            (*marker)->shiftOffsets(delta);
            didShiftMarker = true;
        }
    }

    if (didShiftMarker) {
        invalidateRectsForMarkersInNode(*node);
        // repaint the affected node
        if (node->layoutObject())
            node->layoutObject()->setShouldDoFullPaintInvalidation();
    }
}

bool DocumentMarkerController::setMarkersActive(Range* range, bool active)
{
    if (!possiblyHasMarkers(DocumentMarker::AllMarkers()))
        return false;

    DCHECK(!m_markers.isEmpty());

    Node* startContainer = range->startContainer();
    Node* endContainer = range->endContainer();

    Node* pastLastNode = range->pastLastNode();

    bool markerFound = false;
    for (Node* node = range->firstNode(); node != pastLastNode; node = NodeTraversal::next(*node)) {
        int startOffset = node == startContainer ? range->startOffset() : 0;
        int endOffset = node == endContainer ? range->endOffset() : INT_MAX;
        markerFound |= setMarkersActive(node, startOffset, endOffset, active);
    }
    return markerFound;
}

bool DocumentMarkerController::setMarkersActive(Node* node, unsigned startOffset, unsigned endOffset, bool active)
{
    MarkerLists* markers = m_markers.get(node);
    if (!markers)
        return false;

    bool docDirty = false;
    Member<MarkerList>& list = (*markers)[MarkerTypeToMarkerIndex(DocumentMarker::TextMatch)];
    if (!list)
        return false;
    MarkerList::iterator startPos = std::upper_bound(list->begin(), list->end(), startOffset, endsBefore);
    for (MarkerList::iterator marker = startPos; marker != list->end(); ++marker) {

        // Markers are returned in order, so stop if we are now past the specified range.
        if ((*marker)->startOffset() >= endOffset)
            break;

        (*marker)->setActiveMatch(active);
        docDirty = true;
    }

    // repaint the affected node
    if (docDirty && node->layoutObject())
        node->layoutObject()->setShouldDoFullPaintInvalidation();
    return docDirty;
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
            Member<MarkerList>& list = (*markers)[markerListIndex];
            for (unsigned markerIndex = 0; list.get() && markerIndex < list->size(); ++markerIndex) {
                DocumentMarker* marker = list->at(markerIndex).get();
                fprintf(stderr, " %d:[%d:%d](%d)", marker->type(), marker->startOffset(), marker->endOffset(), marker->activeMatch());
            }
        }

        fprintf(stderr, "\n");
    }
}
#endif

} // namespace blink

#ifndef NDEBUG
void showDocumentMarkers(const blink::DocumentMarkerController* controller)
{
    if (controller)
        controller->showMarkers();
}
#endif
