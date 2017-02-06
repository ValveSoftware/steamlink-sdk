// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DrawingRecorder.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/CachedDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace blink {

bool DrawingRecorder::useCachedDrawingIfPossible(GraphicsContext& context, const DisplayItemClient& client, DisplayItem::Type type)
{
    ASSERT(DisplayItem::isDrawingType(type));

    if (!context.getPaintController().clientCacheIsValid(client))
        return false;

    context.getPaintController().createAndAppend<CachedDisplayItem>(client, DisplayItem::drawingTypeToCachedDrawingType(type));

#if ENABLE(ASSERT)
    // When under-invalidation checking is enabled, we output CachedDrawing display item
    // followed by the display item containing forced painting.
    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled())
        return false;
#endif

    return true;
}

DrawingRecorder::DrawingRecorder(GraphicsContext& context, const DisplayItemClient& displayItemClient, DisplayItem::Type displayItemType, const FloatRect& cullRect)
    : m_context(context)
    , m_displayItemClient(displayItemClient)
    , m_displayItemType(displayItemType)
    , m_knownToBeOpaque(false)
#if ENABLE(ASSERT)
    , m_displayItemPosition(m_context.getPaintController().newDisplayItemList().size())
    , m_underInvalidationCheckingMode(DrawingDisplayItem::CheckPicture)
#endif
{
    if (context.getPaintController().displayItemConstructionIsDisabled())
        return;

    // Must check DrawingRecorder::useCachedDrawingIfPossible before creating the DrawingRecorder.
    DCHECK(RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled()
        || !useCachedDrawingIfPossible(m_context, m_displayItemClient, m_displayItemType));

    ASSERT(DisplayItem::isDrawingType(displayItemType));

#if ENABLE(ASSERT)
    context.setInDrawingRecorder(true);
#endif

    context.beginRecording(cullRect);

#if ENABLE(ASSERT)
    if (RuntimeEnabledFeatures::slimmingPaintStrictCullRectClippingEnabled()) {
        // Skia depends on the cull rect containing all of the display item commands. When strict
        // cull rect clipping is enabled, make this explicit. This allows us to identify potential
        // incorrect cull rects that might otherwise be masked due to Skia internal optimizations.
        context.save();
        IntRect verificationClip = enclosingIntRect(cullRect);
        // Expand the verification clip by one pixel to account for Skia's SkCanvas::getClipBounds()
        // expansion, used in testing cull rects.
        // TODO(schenney) This is not the best place to do this. Ideally, we would expand by one pixel
        // in device (pixel) space, but to do that we would need to add the verification mode to Skia.
        verificationClip.inflate(1);
        context.clipRect(verificationClip, NotAntiAliased, SkRegion::kIntersect_Op);
    }
#endif
}

DrawingRecorder::~DrawingRecorder()
{
    if (m_context.getPaintController().displayItemConstructionIsDisabled())
        return;

#if ENABLE(ASSERT)
    if (RuntimeEnabledFeatures::slimmingPaintStrictCullRectClippingEnabled())
        m_context.restore();

    m_context.setInDrawingRecorder(false);
    ASSERT(m_displayItemPosition == m_context.getPaintController().newDisplayItemList().size());
#endif

    m_context.getPaintController().createAndAppend<DrawingDisplayItem>(m_displayItemClient
        , m_displayItemType
        , m_context.endRecording()
        , m_knownToBeOpaque
#if ENABLE(ASSERT)
        , m_underInvalidationCheckingMode
#endif
        );
}

} // namespace blink
