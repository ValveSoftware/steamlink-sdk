// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintController_h
#define PaintController_h

#include "platform/PlatformExport.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/ContiguousContainer.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/graphics/paint/PaintChunk.h"
#include "platform/graphics/paint/PaintChunker.h"
#include "platform/graphics/paint/Transform3DDisplayItem.h"
#include "wtf/Alignment.h"
#include "wtf/Assertions.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include <memory>
#include <utility>

class SkPicture;

namespace blink {

class GraphicsContext;

static const size_t kInitialDisplayItemListCapacityBytes = 512;

// Responsible for processing display items as they are produced, and producing
// a final paint artifact when complete. This class includes logic for caching,
// cache invalidation, and merging.
class PLATFORM_EXPORT PaintController {
    WTF_MAKE_NONCOPYABLE(PaintController);
    USING_FAST_MALLOC(PaintController);
public:
    static std::unique_ptr<PaintController> create()
    {
        return wrapUnique(new PaintController());
    }

    ~PaintController()
    {
        // New display items should be committed before PaintController is destructed.
        DCHECK(m_newDisplayItemList.isEmpty());
#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
        DisplayItemClient::endShouldKeepAliveAllClients(this);
#endif
    }

    void invalidateAll();

    // These methods are called during painting.

    // Provide a new set of paint chunk properties to apply to recorded display
    // items, for Slimming Paint v2.
    void updateCurrentPaintChunkProperties(const PaintChunkProperties&);

    // Retrieve the current paint properties.
    const PaintChunkProperties& currentPaintChunkProperties() const;

    template <typename DisplayItemClass, typename... Args>
    void createAndAppend(Args&&... args)
    {
        static_assert(WTF::IsSubclass<DisplayItemClass, DisplayItem>::value,
            "Can only createAndAppend subclasses of DisplayItem.");
        static_assert(sizeof(DisplayItemClass) <= kMaximumDisplayItemSize,
            "DisplayItem subclass is larger than kMaximumDisplayItemSize.");

        if (displayItemConstructionIsDisabled())
            return;
        DisplayItemClass& displayItem = m_newDisplayItemList.allocateAndConstruct<DisplayItemClass>(std::forward<Args>(args)...);
        processNewItem(displayItem);
    }

    // Creates and appends an ending display item to pair with a preceding
    // beginning item iff the display item actually draws content. For no-op
    // items, rather than creating an ending item, the begin item will
    // instead be removed, thereby maintaining brevity of the list. If display
    // item construction is disabled, no list mutations will be performed.
    template <typename DisplayItemClass, typename... Args>
    void endItem(Args&&... args)
    {
        if (displayItemConstructionIsDisabled())
            return;
        if (lastDisplayItemIsNoopBegin())
            removeLastDisplayItem();
        else
            createAndAppend<DisplayItemClass>(std::forward<Args>(args)...);
    }

    // True if the last display item is a begin that doesn't draw content.
    bool lastDisplayItemIsNoopBegin() const;
    void removeLastDisplayItem();

    void beginSkippingCache() { ++m_skippingCacheCount; }
    void endSkippingCache() { DCHECK(m_skippingCacheCount > 0); --m_skippingCacheCount; }
    bool skippingCache() const { return m_skippingCacheCount; }

    // Must be called when a painting is finished.
    // offsetFromLayoutObject is the offset between the space of the GraphicsLayer which owns this
    // PaintController and the coordinate space of the owning LayoutObject.
    void commitNewDisplayItems(const LayoutSize& offsetFromLayoutObject = LayoutSize());

    // Returns the approximate memory usage, excluding memory likely to be
    // shared with the embedder after copying to WebPaintController.
    // Should only be called right after commitNewDisplayItems.
    size_t approximateUnsharedMemoryUsage() const;

    // Get the artifact generated after the last commit.
    const PaintArtifact& paintArtifact() const;
    const DisplayItemList& getDisplayItemList() const { return paintArtifact().getDisplayItemList(); }
    const Vector<PaintChunk>& paintChunks() const { return paintArtifact().paintChunks(); }

    bool clientCacheIsValid(const DisplayItemClient&) const;
    bool cacheIsEmpty() const { return m_currentPaintArtifact.isEmpty(); }

    // For micro benchmarking of record time.
    bool displayItemConstructionIsDisabled() const { return m_constructionDisabled; }
    void setDisplayItemConstructionIsDisabled(const bool disable) { m_constructionDisabled = disable; }
    bool subsequenceCachingIsDisabled() const { return m_subsequenceCachingDisabled; }
    void setSubsequenceCachingIsDisabled(bool disable) { m_subsequenceCachingDisabled = disable; }

    bool textPainted() const { return m_textPainted; }
    void setTextPainted() { m_textPainted = true; }
    bool imagePainted() const { return m_imagePainted; }
    void setImagePainted() { m_imagePainted = true; }

    // Returns displayItemList added using createAndAppend() since beginning or
    // the last commitNewDisplayItems(). Use with care.
    DisplayItemList& newDisplayItemList() { return m_newDisplayItemList; }

    void appendDebugDrawingAfterCommit(const DisplayItemClient&, PassRefPtr<SkPicture>, const LayoutSize& offsetFromLayoutObject);

#ifndef NDEBUG
    void showDebugData() const;
#endif

#if DCHECK_IS_ON()
    void assertDisplayItemClientsAreLive();
#endif

protected:
    PaintController()
        : m_newDisplayItemList(kInitialDisplayItemListCapacityBytes)
        , m_constructionDisabled(false)
        , m_subsequenceCachingDisabled(false)
        , m_textPainted(false)
        , m_imagePainted(false)
        , m_skippingCacheCount(0)
        , m_numCachedNewItems(0)
    { }

private:
    // Set new item state (cache skipping, etc) for a new item.
    void processNewItem(DisplayItem&);

#ifndef NDEBUG
    WTF::String displayItemListAsDebugString(const DisplayItemList&) const;
#endif

    // Indices into PaintList of all DrawingDisplayItems and BeginSubsequenceDisplayItems of each client.
    // Temporarily used during merge to find out-of-order display items.
    using DisplayItemIndicesByClientMap = HashMap<const DisplayItemClient*, Vector<size_t>>;

    static size_t findMatchingItemFromIndex(const DisplayItem::Id&, const DisplayItemIndicesByClientMap&, const DisplayItemList&);
    static void addItemToIndexIfNeeded(const DisplayItem&, size_t index, DisplayItemIndicesByClientMap&);

    struct OutOfOrderIndexContext;
    DisplayItemList::iterator findOutOfOrderCachedItem(const DisplayItem::Id&, OutOfOrderIndexContext&);
    DisplayItemList::iterator findOutOfOrderCachedItemForward(const DisplayItem::Id&, OutOfOrderIndexContext&);
    void copyCachedSubsequence(const DisplayItemList& currentList, DisplayItemList::iterator& currentIt, DisplayItemList& updatedList, SkPictureGpuAnalyzer&);

#if DCHECK_IS_ON()
    // The following two methods are for checking under-invalidations
    // (when RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled).
    void checkUnderInvalidation(DisplayItemList::iterator& newIt, DisplayItemList::iterator& currentIt);
    void checkCachedDisplayItemIsUnchanged(const char* messagePrefix, const DisplayItem& newItem, const DisplayItem& oldItem);
#endif

    void updateCacheGeneration();

    // The last complete paint artifact.
    // In SPv2, this includes paint chunks as well as display items.
    PaintArtifact m_currentPaintArtifact;

    // Data being used to build the next paint artifact.
    DisplayItemList m_newDisplayItemList;
    PaintChunker m_newPaintChunks;

    // Allow display item construction to be disabled to isolate the costs of construction
    // in performance metrics.
    bool m_constructionDisabled;

    // Allow subsequence caching to be disabled to test the cost of display item caching.
    bool m_subsequenceCachingDisabled;

    // Indicates this PaintController has ever had text. It is never reset to false.
    bool m_textPainted;
    bool m_imagePainted;

    int m_skippingCacheCount;

    int m_numCachedNewItems;

#if DCHECK_IS_ON()
    // This is used to check duplicated ids during add(). We could also check
    // during commitNewDisplayItems(), but checking during add() helps developer
    // easily find where the duplicated ids are from.
    DisplayItemIndicesByClientMap m_newDisplayItemIndicesByClient;
#endif

    DisplayItemClient::CacheGenerationOrInvalidationReason m_currentCacheGeneration;

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
    // A stack recording subsequence clients that are currently painting.
    Vector<const DisplayItemClient*> m_currentSubsequenceClients;
#endif
};

} // namespace blink

#endif // PaintController_h
