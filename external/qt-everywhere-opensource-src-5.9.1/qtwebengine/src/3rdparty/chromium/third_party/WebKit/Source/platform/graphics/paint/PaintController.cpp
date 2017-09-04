// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintController.h"

#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/tracing/TraceEvent.h"
#include "third_party/skia/include/core/SkPictureAnalyzer.h"
#include "wtf/AutoReset.h"
#include "wtf/text/StringBuilder.h"

#ifndef NDEBUG
#include "platform/graphics/LoggingCanvas.h"
#include <stdio.h>
#endif

namespace blink {

void PaintController::setTracksRasterInvalidations(bool value) {
  if (value) {
    m_paintChunksRasterInvalidationTrackingMap =
        wrapUnique(new RasterInvalidationTrackingMap<const PaintChunk>);
  } else {
    m_paintChunksRasterInvalidationTrackingMap = nullptr;
  }
}

const PaintArtifact& PaintController::paintArtifact() const {
  DCHECK(m_newDisplayItemList.isEmpty());
  DCHECK(m_newPaintChunks.isInInitialState());
  return m_currentPaintArtifact;
}

bool PaintController::useCachedDrawingIfPossible(
    const DisplayItemClient& client,
    DisplayItem::Type type) {
  DCHECK(DisplayItem::isDrawingType(type));

  if (displayItemConstructionIsDisabled())
    return false;

  if (!clientCacheIsValid(client))
    return false;

  if (RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled() &&
      isCheckingUnderInvalidation()) {
    // We are checking under-invalidation of a subsequence enclosing this
    // display item. Let the client continue to actually paint the display item.
    return false;
  }

  size_t cachedItem = findCachedItem(DisplayItem::Id(client, type));
  if (cachedItem == kNotFound) {
    NOTREACHED();
    return false;
  }

  ++m_numCachedNewItems;
  ensureNewDisplayItemListInitialCapacity();
  if (!RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled())
    processNewItem(moveItemFromCurrentListToNewList(cachedItem));

  m_nextItemToMatch = cachedItem + 1;
  // Items before m_nextItemToMatch have been copied so we don't need to index
  // them.
  if (m_nextItemToMatch > m_nextItemToIndex)
    m_nextItemToIndex = m_nextItemToMatch;

  if (RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled()) {
    if (!isCheckingUnderInvalidation()) {
      m_underInvalidationCheckingBegin = cachedItem;
      m_underInvalidationCheckingEnd = cachedItem + 1;
      m_underInvalidationMessagePrefix = "";
    }
    // Return false to let the painter actually paint. We will check if the new
    // painting is the same as the cached one.
    return false;
  }

  return true;
}

bool PaintController::useCachedSubsequenceIfPossible(
    const DisplayItemClient& client) {
  if (displayItemConstructionIsDisabled() || subsequenceCachingIsDisabled())
    return false;

  if (!clientCacheIsValid(client))
    return false;

  if (RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled() &&
      isCheckingUnderInvalidation()) {
    // We are checking under-invalidation of an ancestor subsequence enclosing
    // this one. The ancestor subsequence is supposed to have already "copied",
    // so we should let the client continue to actually paint the descendant
    // subsequences without "copying".
    return false;
  }

  size_t cachedItem =
      findCachedItem(DisplayItem::Id(client, DisplayItem::kSubsequence));
  if (cachedItem == kNotFound) {
    NOTREACHED();
    return false;
  }

  // |cachedItem| will point to the first item after the subsequence or end of
  // the current list.
  ensureNewDisplayItemListInitialCapacity();
  copyCachedSubsequence(cachedItem);

  m_nextItemToMatch = cachedItem;
  // Items before |cachedItem| have been copied so we don't need to index them.
  if (cachedItem > m_nextItemToIndex)
    m_nextItemToIndex = cachedItem;

  if (RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled()) {
    // Return false to let the painter actually paint. We will check if the new
    // painting is the same as the cached one.
    return false;
  }

  return true;
}

bool PaintController::lastDisplayItemIsNoopBegin() const {
  if (m_newDisplayItemList.isEmpty())
    return false;

  const auto& lastDisplayItem = m_newDisplayItemList.last();
  return lastDisplayItem.isBegin() && !lastDisplayItem.drawsContent();
}

void PaintController::removeLastDisplayItem() {
  if (m_newDisplayItemList.isEmpty())
    return;

#if DCHECK_IS_ON()
  // Also remove the index pointing to the removed display item.
  IndicesByClientMap::iterator it = m_newDisplayItemIndicesByClient.find(
      &m_newDisplayItemList.last().client());
  if (it != m_newDisplayItemIndicesByClient.end()) {
    Vector<size_t>& indices = it->value;
    if (!indices.isEmpty() &&
        indices.last() == (m_newDisplayItemList.size() - 1))
      indices.pop_back();
  }
#endif

  if (RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled() &&
      isCheckingUnderInvalidation()) {
    if (m_skippedProbableUnderInvalidationCount) {
      --m_skippedProbableUnderInvalidationCount;
    } else {
      DCHECK(m_underInvalidationCheckingBegin);
      --m_underInvalidationCheckingBegin;
    }
  }
  m_newDisplayItemList.removeLast();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    m_newPaintChunks.decrementDisplayItemIndex();
}

const DisplayItem* PaintController::lastDisplayItem(unsigned offset) {
  if (offset < m_newDisplayItemList.size())
    return &m_newDisplayItemList[m_newDisplayItemList.size() - offset - 1];
  return nullptr;
}

void PaintController::processNewItem(DisplayItem& displayItem) {
  DCHECK(!m_constructionDisabled);

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
  if (!isSkippingCache()) {
    if (displayItem.isCacheable()) {
      // Mark the client shouldKeepAlive under this PaintController.
      // The status will end after the new display items are committed.
      displayItem.client().beginShouldKeepAlive(this);

      if (!m_currentSubsequenceClients.isEmpty()) {
        // Mark the client shouldKeepAlive under the current subsequence.
        // The status will end when the subsequence owner is invalidated or
        // deleted.
        displayItem.client().beginShouldKeepAlive(
            m_currentSubsequenceClients.last());
      }
    }

    if (displayItem.getType() == DisplayItem::kSubsequence) {
      m_currentSubsequenceClients.append(&displayItem.client());
    } else if (displayItem.getType() == DisplayItem::kEndSubsequence) {
      CHECK(m_currentSubsequenceClients.last() == &displayItem.client());
      m_currentSubsequenceClients.pop_back();
    }
  }
#endif

  if (isSkippingCache())
    displayItem.setSkippedCache();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    size_t lastChunkIndex = m_newPaintChunks.lastChunkIndex();
    if (m_newPaintChunks.incrementDisplayItemIndex(displayItem)) {
      DCHECK(lastChunkIndex != m_newPaintChunks.lastChunkIndex());
      if (lastChunkIndex != kNotFound)
        generateChunkRasterInvalidationRects(
            m_newPaintChunks.paintChunkAt(lastChunkIndex));
    }
  }

#if DCHECK_IS_ON()
  // Verify noop begin/end pairs have been removed.
  if (m_newDisplayItemList.size() >= 2 && displayItem.isEnd()) {
    const auto& beginDisplayItem =
        m_newDisplayItemList[m_newDisplayItemList.size() - 2];
    if (beginDisplayItem.isBegin() &&
        beginDisplayItem.getType() != DisplayItem::kSubsequence &&
        !beginDisplayItem.drawsContent())
      DCHECK(!displayItem.isEndAndPairedWith(beginDisplayItem.getType()));
  }

  size_t index = findMatchingItemFromIndex(displayItem.getId(),
                                           m_newDisplayItemIndicesByClient,
                                           m_newDisplayItemList);
  if (index != kNotFound) {
#ifndef NDEBUG
    showDebugData();
    WTFLogAlways(
        "DisplayItem %s has duplicated id with previous %s (index=%zu)\n",
        displayItem.asDebugString().utf8().data(),
        m_newDisplayItemList[index].asDebugString().utf8().data(), index);
#endif
    NOTREACHED();
  }
  addItemToIndexIfNeeded(displayItem, m_newDisplayItemList.size() - 1,
                         m_newDisplayItemIndicesByClient);
#endif  // DCHECK_IS_ON()

  if (RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled())
    checkUnderInvalidation();
}

DisplayItem& PaintController::moveItemFromCurrentListToNewList(size_t index) {
  m_itemsMovedIntoNewList.resize(
      m_currentPaintArtifact.getDisplayItemList().size());
  m_itemsMovedIntoNewList[index] = m_newDisplayItemList.size();
  return m_newDisplayItemList.appendByMoving(
      m_currentPaintArtifact.getDisplayItemList()[index]);
}

void PaintController::updateCurrentPaintChunkProperties(
    const PaintChunk::Id* id,
    const PaintChunkProperties& newProperties) {
  m_newPaintChunks.updateCurrentPaintChunkProperties(id, newProperties);
}

const PaintChunkProperties& PaintController::currentPaintChunkProperties()
    const {
  return m_newPaintChunks.currentPaintChunkProperties();
}

void PaintController::invalidateAll() {
  // Can only be called during layout/paintInvalidation, not during painting.
  DCHECK(m_newDisplayItemList.isEmpty());
  m_currentPaintArtifact.reset();
  m_currentCacheGeneration.invalidate();
}

bool PaintController::clientCacheIsValid(
    const DisplayItemClient& client) const {
#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
  CHECK(client.isAlive());
#endif
  if (isSkippingCache())
    return false;
  return client.displayItemsAreCached(m_currentCacheGeneration);
}

size_t PaintController::findMatchingItemFromIndex(
    const DisplayItem::Id& id,
    const IndicesByClientMap& displayItemIndicesByClient,
    const DisplayItemList& list) {
  IndicesByClientMap::const_iterator it =
      displayItemIndicesByClient.find(&id.client);
  if (it == displayItemIndicesByClient.end())
    return kNotFound;

  const Vector<size_t>& indices = it->value;
  for (size_t index : indices) {
    const DisplayItem& existingItem = list[index];
    if (!existingItem.hasValidClient())
      continue;
    DCHECK(existingItem.client() == id.client);
    if (id == existingItem.getId())
      return index;
  }

  return kNotFound;
}

void PaintController::addItemToIndexIfNeeded(
    const DisplayItem& displayItem,
    size_t index,
    IndicesByClientMap& displayItemIndicesByClient) {
  if (!displayItem.isCacheable())
    return;

  IndicesByClientMap::iterator it =
      displayItemIndicesByClient.find(&displayItem.client());
  Vector<size_t>& indices =
      it == displayItemIndicesByClient.end()
          ? displayItemIndicesByClient
                .add(&displayItem.client(), Vector<size_t>())
                .storedValue->value
          : it->value;
  indices.append(index);
}

size_t PaintController::findCachedItem(const DisplayItem::Id& id) {
  DCHECK(clientCacheIsValid(id.client));

  // Try to find the item sequentially first. This is fast if the current list
  // and the new list are in the same order around the new item. If found, we
  // don't need to update and lookup the index.
  for (size_t i = m_nextItemToMatch;
       i < m_currentPaintArtifact.getDisplayItemList().size(); ++i) {
    // We encounter an item that has already been copied which indicates we
    // can't do sequential matching.
    const DisplayItem& item = m_currentPaintArtifact.getDisplayItemList()[i];
    if (!item.hasValidClient())
      break;
    if (id == item.getId()) {
#ifndef NDEBUG
      ++m_numSequentialMatches;
#endif
      return i;
    }
    // We encounter a different cacheable item which also indicates we can't do
    // sequential matching.
    if (item.isCacheable())
      break;
  }

  size_t foundIndex = findMatchingItemFromIndex(
      id, m_outOfOrderItemIndices, m_currentPaintArtifact.getDisplayItemList());
  if (foundIndex != kNotFound) {
#ifndef NDEBUG
    ++m_numOutOfOrderMatches;
#endif
    return foundIndex;
  }

  return findOutOfOrderCachedItemForward(id);
}

// Find forward for the item and index all skipped indexable items.
size_t PaintController::findOutOfOrderCachedItemForward(
    const DisplayItem::Id& id) {
  for (size_t i = m_nextItemToIndex;
       i < m_currentPaintArtifact.getDisplayItemList().size(); ++i) {
    const DisplayItem& item = m_currentPaintArtifact.getDisplayItemList()[i];
    DCHECK(item.hasValidClient());
    if (id == item.getId()) {
#ifndef NDEBUG
      ++m_numSequentialMatches;
#endif
      return i;
    }
    if (item.isCacheable()) {
#ifndef NDEBUG
      ++m_numIndexedItems;
#endif
      addItemToIndexIfNeeded(item, i, m_outOfOrderItemIndices);
    }
  }

#ifndef NDEBUG
  showDebugData();
  LOG(ERROR) << id.client.debugName() << ":"
             << DisplayItem::typeAsDebugString(id.type);
#endif

  if (RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled())
    CHECK(false) << "Can't find cached display item";

  // We did not find the cached display item. This should be impossible, but may
  // occur if there is a bug in the system, such as under-invalidation,
  // incorrect cache checking or duplicate display ids. In this case, the caller
  // should fall back to repaint the display item.
  return kNotFound;
}

// Copies a cached subsequence from current list to the new list. On return,
// |cachedItemIndex| points to the item after the EndSubsequence item of the
// subsequence. When paintUnderInvaldiationCheckingEnabled() we'll not actually
// copy the subsequence, but mark the begin and end of the subsequence for
// under-invalidation checking.
void PaintController::copyCachedSubsequence(size_t& cachedItemIndex) {
  AutoReset<size_t> subsequenceBeginIndex(
      &m_currentCachedSubsequenceBeginIndexInNewList,
      m_newDisplayItemList.size());
  DisplayItem* cachedItem =
      &m_currentPaintArtifact.getDisplayItemList()[cachedItemIndex];
  DCHECK(cachedItem->getType() == DisplayItem::kSubsequence);

  if (RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled()) {
    DCHECK(!isCheckingUnderInvalidation());
    m_underInvalidationCheckingBegin = cachedItemIndex;
    m_underInvalidationMessagePrefix =
        "(In cached subsequence of " + cachedItem->client().debugName() + ")";
  }

  DisplayItem::Id endSubsequenceId(cachedItem->client(),
                                   DisplayItem::kEndSubsequence);
  Vector<PaintChunk>::const_iterator cachedChunk;
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    cachedChunk =
        m_currentPaintArtifact.findChunkByDisplayItemIndex(cachedItemIndex);
    DCHECK(cachedChunk != m_currentPaintArtifact.paintChunks().end());
    updateCurrentPaintChunkProperties(
        cachedChunk->id ? &*cachedChunk->id : nullptr, cachedChunk->properties);
  } else {
    // Avoid uninitialized variable error on Windows.
    cachedChunk = m_currentPaintArtifact.paintChunks().begin();
  }

  while (true) {
    DCHECK(cachedItem->hasValidClient());
#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
    CHECK(cachedItem->client().isAlive());
#endif
    ++m_numCachedNewItems;
    bool metEndSubsequence = cachedItem->getId() == endSubsequenceId;
    if (!RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled()) {
      if (RuntimeEnabledFeatures::slimmingPaintV2Enabled() &&
          cachedItemIndex == cachedChunk->endIndex) {
        ++cachedChunk;
        DCHECK(cachedChunk != m_currentPaintArtifact.paintChunks().end());
        updateCurrentPaintChunkProperties(
            cachedChunk->id ? &*cachedChunk->id : nullptr,
            cachedChunk->properties);
      }
      processNewItem(moveItemFromCurrentListToNewList(cachedItemIndex));
      if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
        DCHECK((!m_newPaintChunks.lastChunk().id && !cachedChunk->id) ||
               m_newPaintChunks.lastChunk().matches(*cachedChunk));
    }

    ++cachedItemIndex;
    if (metEndSubsequence)
      break;

    // We should always be able to find the EndSubsequence display item.
    DCHECK(cachedItemIndex <
           m_currentPaintArtifact.getDisplayItemList().size());
    cachedItem = &m_currentPaintArtifact.getDisplayItemList()[cachedItemIndex];
  }

  if (RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled()) {
    m_underInvalidationCheckingEnd = cachedItemIndex;
    DCHECK(isCheckingUnderInvalidation());
  }
}

static IntRect visualRectForDisplayItem(
    const DisplayItem& displayItem,
    const LayoutSize& offsetFromLayoutObject) {
  LayoutRect visualRect = displayItem.client().visualRect();
  visualRect.move(-offsetFromLayoutObject);
  return enclosingIntRect(visualRect);
}

void PaintController::resetCurrentListIndices() {
  m_nextItemToMatch = 0;
  m_nextItemToIndex = 0;
  m_nextChunkToMatch = 0;
  m_underInvalidationCheckingBegin = 0;
  m_underInvalidationCheckingEnd = 0;
  m_skippedProbableUnderInvalidationCount = 0;
}

void PaintController::commitNewDisplayItems(
    const LayoutSize& offsetFromLayoutObject) {
  TRACE_EVENT2("blink,benchmark", "PaintController::commitNewDisplayItems",
               "current_display_list_size",
               (int)m_currentPaintArtifact.getDisplayItemList().size(),
               "num_non_cached_new_items",
               (int)m_newDisplayItemList.size() - m_numCachedNewItems);
  m_numCachedNewItems = 0;

  // These data structures are used during painting only.
  DCHECK(!isSkippingCache());
#if DCHECK_IS_ON()
  m_newDisplayItemIndicesByClient.clear();
#endif

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled() &&
      !m_newDisplayItemList.isEmpty())
    generateChunkRasterInvalidationRects(m_newPaintChunks.lastChunk());

  SkPictureGpuAnalyzer gpuAnalyzer;

  m_currentCacheGeneration =
      DisplayItemClient::CacheGenerationOrInvalidationReason::next();
  Vector<const DisplayItemClient*> skippedCacheClients;
  for (const auto& item : m_newDisplayItemList) {
    // No reason to continue the analysis once we have a veto.
    if (gpuAnalyzer.suitableForGpuRasterization())
      item.analyzeForGpuRasterization(gpuAnalyzer);

    // TODO(wkorman): Only compute and append visual rect for drawings.
    m_newDisplayItemList.appendVisualRect(
        visualRectForDisplayItem(item, offsetFromLayoutObject));

    if (item.isCacheable()) {
      item.client().setDisplayItemsCached(m_currentCacheGeneration);
    } else {
      if (item.client().isJustCreated())
        item.client().clearIsJustCreated();
      if (item.skippedCache())
        skippedCacheClients.append(&item.client());
    }
  }

  for (auto* client : skippedCacheClients)
    client->setDisplayItemsUncached();

  // The new list will not be appended to again so we can release unused memory.
  m_newDisplayItemList.shrinkToFit();
  m_currentPaintArtifact = PaintArtifact(
      std::move(m_newDisplayItemList), m_newPaintChunks.releasePaintChunks(),
      gpuAnalyzer.suitableForGpuRasterization());
  resetCurrentListIndices();
  m_outOfOrderItemIndices.clear();
  m_outOfOrderChunkIndices.clear();
  m_itemsMovedIntoNewList.clear();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    for (const auto& chunk : m_currentPaintArtifact.paintChunks()) {
      if (chunk.id && chunk.id->client.isJustCreated())
        chunk.id->client.clearIsJustCreated();
    }
  }

  // We'll allocate the initial buffer when we start the next paint.
  m_newDisplayItemList = DisplayItemList(0);

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
  CHECK(m_currentSubsequenceClients.isEmpty());
  DisplayItemClient::endShouldKeepAliveAllClients(this);
#endif

#ifndef NDEBUG
  m_numSequentialMatches = 0;
  m_numOutOfOrderMatches = 0;
  m_numIndexedItems = 0;
#endif
}

size_t PaintController::approximateUnsharedMemoryUsage() const {
  size_t memoryUsage = sizeof(*this);

  // Memory outside this class due to m_currentPaintArtifact.
  memoryUsage += m_currentPaintArtifact.approximateUnsharedMemoryUsage() -
                 sizeof(m_currentPaintArtifact);

  // TODO(jbroman): If display items begin to have significant external memory
  // usage that's not shared with the embedder, we should account for it here.
  //
  // External objects, shared with the embedder, such as SkPicture, should be
  // excluded to avoid double counting. It is the embedder's responsibility to
  // count such objects.
  //
  // At time of writing, the only known case of unshared external memory was
  // the rounded clips vector in ClipDisplayItem, which is not expected to
  // contribute significantly to memory usage.

  // Memory outside this class due to m_newDisplayItemList.
  DCHECK(m_newDisplayItemList.isEmpty());
  memoryUsage += m_newDisplayItemList.memoryUsageInBytes();

  return memoryUsage;
}

void PaintController::appendDebugDrawingAfterCommit(
    const DisplayItemClient& displayItemClient,
    sk_sp<SkPicture> picture,
    const LayoutSize& offsetFromLayoutObject) {
  DCHECK(m_newDisplayItemList.isEmpty());
  DrawingDisplayItem& displayItem =
      m_currentPaintArtifact.getDisplayItemList()
          .allocateAndConstruct<DrawingDisplayItem>(displayItemClient,
                                                    DisplayItem::kDebugDrawing,
                                                    std::move(picture));
  displayItem.setSkippedCache();
  // TODO(wkorman): Only compute and append visual rect for drawings.
  m_currentPaintArtifact.getDisplayItemList().appendVisualRect(
      visualRectForDisplayItem(displayItem, offsetFromLayoutObject));
}

void PaintController::generateChunkRasterInvalidationRects(
    PaintChunk& newChunk) {
  DCHECK(RuntimeEnabledFeatures::slimmingPaintV2Enabled());
  if (newChunk.beginIndex >= m_currentCachedSubsequenceBeginIndexInNewList)
    return;

  static FloatRect infiniteFloatRect(LayoutRect::infiniteIntRect());
  if (!newChunk.id) {
    addRasterInvalidationInfo(nullptr, newChunk, infiniteFloatRect);
    return;
  }

  // Try to match old chunk sequentially first.
  const auto& oldChunks = m_currentPaintArtifact.paintChunks();
  while (m_nextChunkToMatch < oldChunks.size()) {
    const PaintChunk& oldChunk = oldChunks[m_nextChunkToMatch];
    if (newChunk.matches(oldChunk)) {
      generateChunkRasterInvalidationRectsComparingOldChunk(newChunk, oldChunk);
      ++m_nextChunkToMatch;
      return;
    }

    // Add skipped old chunks into the index.
    if (oldChunk.id) {
      auto it = m_outOfOrderChunkIndices.find(&oldChunk.id->client);
      Vector<size_t>& indices =
          it == m_outOfOrderChunkIndices.end()
              ? m_outOfOrderChunkIndices
                    .add(&oldChunk.id->client, Vector<size_t>())
                    .storedValue->value
              : it->value;
      indices.append(m_nextChunkToMatch);
    }
    ++m_nextChunkToMatch;
  }

  // Sequential matching reaches the end. Find from the out-of-order index.
  auto it = m_outOfOrderChunkIndices.find(&newChunk.id->client);
  if (it != m_outOfOrderChunkIndices.end()) {
    for (size_t i : it->value) {
      if (newChunk.matches(oldChunks[i])) {
        generateChunkRasterInvalidationRectsComparingOldChunk(newChunk,
                                                              oldChunks[i]);
        return;
      }
    }
  }

  // We reach here because the chunk is new.
  addRasterInvalidationInfo(nullptr, newChunk, infiniteFloatRect);
}

void PaintController::addRasterInvalidationInfo(const DisplayItemClient* client,
                                                PaintChunk& chunk,
                                                const FloatRect& rect) {
  chunk.rasterInvalidationRects.append(rect);
  if (!m_paintChunksRasterInvalidationTrackingMap)
    return;
  RasterInvalidationInfo info;
  info.rect = enclosingIntRect(rect);
  info.client = client;
  if (client) {
    info.clientDebugName = client->debugName();
    info.reason = client->getPaintInvalidationReason();
  }
  RasterInvalidationTracking& tracking =
      m_paintChunksRasterInvalidationTrackingMap->add(&chunk);
  tracking.trackedRasterInvalidations.append(info);
}

void PaintController::generateChunkRasterInvalidationRectsComparingOldChunk(
    PaintChunk& newChunk,
    const PaintChunk& oldChunk) {
  DCHECK(RuntimeEnabledFeatures::slimmingPaintV2Enabled());

  // TODO(wangxianzhu): Handle PaintInvalidationIncremental.
  // TODO(wangxianzhu): Optimize paint offset change.

  HashSet<const DisplayItemClient*> invalidatedClientsInOldChunk;
  size_t highestMovedToIndex = 0;
  for (size_t oldIndex = oldChunk.beginIndex; oldIndex < oldChunk.endIndex;
       ++oldIndex) {
    const DisplayItem& oldItem =
        m_currentPaintArtifact.getDisplayItemList()[oldIndex];
    const DisplayItemClient* clientToInvalidate = nullptr;
    bool isPotentiallyInvalidClient = false;
    if (!oldItem.hasValidClient()) {
      size_t movedToIndex = m_itemsMovedIntoNewList[oldIndex];
      if (m_newDisplayItemList[movedToIndex].drawsContent()) {
        if (movedToIndex < newChunk.beginIndex ||
            movedToIndex >= newChunk.endIndex) {
          // The item has been moved into another chunk, so need to invalidate
          // it in the old chunk.
          clientToInvalidate = &m_newDisplayItemList[movedToIndex].client();
          // And invalidate in the new chunk into which the item was moved.
          PaintChunk& movedToChunk =
              m_newPaintChunks.findChunkByDisplayItemIndex(movedToIndex);
          addRasterInvalidationInfo(
              clientToInvalidate, movedToChunk,
              FloatRect(clientToInvalidate->visualRect()));
        } else if (movedToIndex < highestMovedToIndex) {
          // The item has been moved behind other cached items, so need to
          // invalidate the area that is probably exposed by the item moved
          // earlier.
          clientToInvalidate = &m_newDisplayItemList[movedToIndex].client();
        } else {
          highestMovedToIndex = movedToIndex;
        }
      }
    } else if (oldItem.drawsContent()) {
      isPotentiallyInvalidClient = true;
      clientToInvalidate = &oldItem.client();
    }
    if (clientToInvalidate &&
        invalidatedClientsInOldChunk.add(clientToInvalidate).isNewEntry) {
      addRasterInvalidationInfo(
          isPotentiallyInvalidClient ? nullptr : clientToInvalidate, newChunk,
          FloatRect(m_currentPaintArtifact.getDisplayItemList().visualRect(
              oldIndex)));
    }
  }

  HashSet<const DisplayItemClient*> invalidatedClientsInNewChunk;
  for (size_t newIndex = newChunk.beginIndex; newIndex < newChunk.endIndex;
       ++newIndex) {
    const DisplayItem& newItem = m_newDisplayItemList[newIndex];
    if (newItem.drawsContent() && !clientCacheIsValid(newItem.client()) &&
        invalidatedClientsInNewChunk.add(&newItem.client()).isNewEntry) {
      addRasterInvalidationInfo(&newItem.client(), newChunk,
                                FloatRect(newItem.client().visualRect()));
    }
  }
}

void PaintController::showUnderInvalidationError(
    const char* reason,
    const DisplayItem& newItem,
    const DisplayItem* oldItem) const {
  LOG(ERROR) << m_underInvalidationMessagePrefix << " " << reason;
#ifndef NDEBUG
  LOG(ERROR) << "New display item: " << newItem.asDebugString();
  LOG(ERROR) << "Old display item: "
             << (oldItem ? oldItem->asDebugString() : "None");
#else
  LOG(ERROR) << "Run debug build to get more details.";
#endif
  LOG(ERROR) << "See http://crbug.com/619103.";

#ifndef NDEBUG
  const SkPicture* newPicture =
      newItem.isDrawing()
          ? static_cast<const DrawingDisplayItem&>(newItem).picture()
          : nullptr;
  const SkPicture* oldPicture =
      oldItem && oldItem->isDrawing()
          ? static_cast<const DrawingDisplayItem*>(oldItem)->picture()
          : nullptr;
  LOG(INFO) << "new picture:\n"
            << (newPicture ? pictureAsDebugString(newPicture) : "None");
  LOG(INFO) << "old picture:\n"
            << (oldPicture ? pictureAsDebugString(oldPicture) : "None");

  showDebugData();
#endif  // NDEBUG
}

void PaintController::checkUnderInvalidation() {
  DCHECK(RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled());

  if (!isCheckingUnderInvalidation())
    return;

  const DisplayItem& newItem = m_newDisplayItemList.last();
  size_t oldItemIndex = m_underInvalidationCheckingBegin +
                        m_skippedProbableUnderInvalidationCount;
  DisplayItem* oldItem =
      oldItemIndex < m_currentPaintArtifact.getDisplayItemList().size()
          ? &m_currentPaintArtifact.getDisplayItemList()[oldItemIndex]
          : nullptr;

  bool oldAndNewEqual = oldItem && newItem.equals(*oldItem);
  if (!oldAndNewEqual) {
    if (newItem.isBegin()) {
      // Temporarily skip mismatching begin display item which may be removed
      // when we remove a no-op pair.
      ++m_skippedProbableUnderInvalidationCount;
      return;
    }
    if (newItem.isDrawing() && m_skippedProbableUnderInvalidationCount == 1) {
      DCHECK_GE(m_newDisplayItemList.size(), 2u);
      if (m_newDisplayItemList[m_newDisplayItemList.size() - 2].getType() ==
          DisplayItem::kBeginCompositing) {
        // This might be a drawing item between a pair of begin/end compositing
        // display items that will be folded into a single drawing display item.
        ++m_skippedProbableUnderInvalidationCount;
        return;
      }
    }
  }

  if (m_skippedProbableUnderInvalidationCount || !oldAndNewEqual) {
    // If we ever skipped reporting any under-invalidations, report the earliest
    // one.
    showUnderInvalidationError(
        "under-invalidation: display item changed",
        m_newDisplayItemList[m_newDisplayItemList.size() -
                             m_skippedProbableUnderInvalidationCount - 1],
        &m_currentPaintArtifact
             .getDisplayItemList()[m_underInvalidationCheckingBegin]);
    CHECK(false);
  }

  // Discard the forced repainted display item and move the cached item into
  // m_newDisplayItemList. This is to align with the
  // non-under-invalidation-checking path to empty the original cached slot,
  // leaving only disappeared or invalidated display items in the old list after
  // painting.
  m_newDisplayItemList.removeLast();
  moveItemFromCurrentListToNewList(oldItemIndex);

  ++m_underInvalidationCheckingBegin;
}

String PaintController::displayItemListAsDebugString(
    const DisplayItemList& list,
    bool showPictures) const {
  StringBuilder stringBuilder;
  size_t i = 0;
  for (auto it = list.begin(); it != list.end(); ++it, ++i) {
    const DisplayItem& displayItem = *it;
    if (i)
      stringBuilder.append(",\n");
    stringBuilder.append(String::format("{index: %zu, ", i));
#ifndef NDEBUG
    displayItem.dumpPropertiesAsDebugString(stringBuilder);
#endif

    if (displayItem.hasValidClient()) {
#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
      if (!displayItem.client().isAlive()) {
        stringBuilder.append(", clientIsAlive: false");
      } else {
#else
      // debugName() and clientCacheIsValid() can only be called on a live
      // client, so only output it for m_newDisplayItemList, in which we are
      // sure the clients are all alive.
      if (&list == &m_newDisplayItemList) {
#endif
#ifdef NDEBUG
        stringBuilder.append(
            String::format("clientDebugName: \"%s\"",
                           displayItem.client().debugName().ascii().data()));
#endif
        stringBuilder.append(", cacheIsValid: ");
        stringBuilder.append(
            clientCacheIsValid(displayItem.client()) ? "true" : "false");
      }
#ifndef NDEBUG
      if (showPictures && displayItem.isDrawing()) {
        if (const SkPicture* picture =
                static_cast<const DrawingDisplayItem&>(displayItem).picture()) {
          stringBuilder.append(", picture: ");
          stringBuilder.append(pictureAsDebugString(picture));
        }
      }
#endif
    }
    if (list.hasVisualRect(i)) {
      IntRect visualRect = list.visualRect(i);
      stringBuilder.append(String::format(
          ", visualRect: [%d,%d %dx%d]", visualRect.x(), visualRect.y(),
          visualRect.width(), visualRect.height()));
    }
    stringBuilder.append('}');
  }
  return stringBuilder.toString();
}

void PaintController::showDebugDataInternal(bool showPictures) const {
  WTFLogAlways("current display item list: [%s]\n",
               displayItemListAsDebugString(
                   m_currentPaintArtifact.getDisplayItemList(), showPictures)
                   .utf8()
                   .data());
  WTFLogAlways("new display item list: [%s]\n",
               displayItemListAsDebugString(m_newDisplayItemList, showPictures)
                   .utf8()
                   .data());
}

}  // namespace blink
