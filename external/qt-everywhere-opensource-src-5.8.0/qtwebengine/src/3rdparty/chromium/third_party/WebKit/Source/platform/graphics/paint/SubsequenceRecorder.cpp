// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/SubsequenceRecorder.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/CachedDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/SubsequenceDisplayItem.h"

namespace blink {

bool SubsequenceRecorder::useCachedSubsequenceIfPossible(GraphicsContext& context, const DisplayItemClient& client)
{
    if (context.getPaintController().displayItemConstructionIsDisabled() || context.getPaintController().subsequenceCachingIsDisabled())
        return false;

    if (!context.getPaintController().clientCacheIsValid(client))
        return false;

    // TODO(pdr): Implement subsequence caching for spv2 (crbug.com/596983).
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
        return false;

    context.getPaintController().createAndAppend<CachedDisplayItem>(client, DisplayItem::CachedSubsequence);

#if ENABLE(ASSERT)
    // When under-invalidation checking is enabled, we output CachedSubsequence display item
    // followed by forced painting of the subsequence.
    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled())
        return false;
#endif

    return true;
}

SubsequenceRecorder::SubsequenceRecorder(GraphicsContext& context, const DisplayItemClient& client)
    : m_paintController(context.getPaintController())
    , m_client(client)
    , m_beginSubsequenceIndex(0)
{
    if (m_paintController.displayItemConstructionIsDisabled())
        return;

    m_beginSubsequenceIndex = m_paintController.newDisplayItemList().size();
    m_paintController.createAndAppend<BeginSubsequenceDisplayItem>(m_client);
}

SubsequenceRecorder::~SubsequenceRecorder()
{
    if (m_paintController.displayItemConstructionIsDisabled())
        return;

    if (m_paintController.lastDisplayItemIsNoopBegin()) {
        ASSERT(m_beginSubsequenceIndex == m_paintController.newDisplayItemList().size() - 1);
        // Remove uncacheable no-op BeginSubsequence/EndSubsequence pairs.
        // Don't remove cacheable no-op pairs because we need to match them later with CachedSubsequences.
        if (m_paintController.newDisplayItemList().last().skippedCache()) {
            m_paintController.removeLastDisplayItem();
            return;
        }
    }

    m_paintController.createAndAppend<EndSubsequenceDisplayItem>(m_client);
}

} // namespace blink
