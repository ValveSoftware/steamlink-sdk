// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/CompositingRecorder.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/CompositingDisplayItem.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/SkPictureBuilder.h"

namespace blink {

CompositingRecorder::CompositingRecorder(GraphicsContext& graphicsContext,
                                         const DisplayItemClient& client,
                                         const SkBlendMode xferMode,
                                         const float opacity,
                                         const FloatRect* bounds,
                                         ColorFilter colorFilter)
    : m_client(client), m_graphicsContext(graphicsContext) {
  beginCompositing(graphicsContext, m_client, xferMode, opacity, bounds,
                   colorFilter);
}

CompositingRecorder::~CompositingRecorder() {
  endCompositing(m_graphicsContext, m_client);
}

void CompositingRecorder::beginCompositing(GraphicsContext& graphicsContext,
                                           const DisplayItemClient& client,
                                           const SkBlendMode xferMode,
                                           const float opacity,
                                           const FloatRect* bounds,
                                           ColorFilter colorFilter) {
  graphicsContext.getPaintController()
      .createAndAppend<BeginCompositingDisplayItem>(client, xferMode, opacity,
                                                    bounds, colorFilter);
}

void CompositingRecorder::endCompositing(GraphicsContext& graphicsContext,
                                         const DisplayItemClient& client) {
  // If the end of the current display list is of the form
  // [BeginCompositingDisplayItem] [DrawingDisplayItem], then fold the
  // BeginCompositingDisplayItem into a new DrawingDisplayItem that replaces
  // them both. This allows Skia to optimize for the case when the
  // BeginCompositingDisplayItem represents a simple opacity/color that can be
  // merged into the opacity/color of the drawing. See crbug.com/628831 for more
  // details.
  PaintController& paintController = graphicsContext.getPaintController();
  const DisplayItem* lastDisplayItem = paintController.lastDisplayItem(0);
  const DisplayItem* secondToLastDisplayItem =
      paintController.lastDisplayItem(1);
  if (lastDisplayItem && secondToLastDisplayItem &&
      lastDisplayItem->drawsContent() &&
      secondToLastDisplayItem->getType() == DisplayItem::kBeginCompositing) {
    FloatRect cullRect(
        ((DrawingDisplayItem*)lastDisplayItem)->picture()->cullRect());
    const DisplayItemClient& displayItemClient = lastDisplayItem->client();
    DisplayItem::Type displayItemType = lastDisplayItem->getType();

    // Re-record the last two DisplayItems into a new SkPicture.
    SkPictureBuilder pictureBuilder(cullRect, nullptr, &graphicsContext);
    {
#if DCHECK_IS_ON()
      // The picture builder creates an internal paint controller that has been
      // initialized with null paint properties. Painting into this controller
      // without properties will not cause problems because the display item
      // from this internal paint controller is immediately reunited with the
      // correct properties.
      DisableNullPaintPropertyChecks disabler;
#endif
      DrawingRecorder newRecorder(pictureBuilder.context(), displayItemClient,
                                  displayItemType, cullRect);
      DCHECK(!DrawingRecorder::useCachedDrawingIfPossible(
          pictureBuilder.context(), displayItemClient, displayItemType));

      secondToLastDisplayItem->replay(pictureBuilder.context());
      lastDisplayItem->replay(pictureBuilder.context());
      EndCompositingDisplayItem(client).replay(pictureBuilder.context());
    }

    paintController.removeLastDisplayItem();  // Remove the DrawingDisplayItem.
    paintController
        .removeLastDisplayItem();  // Remove the BeginCompositingDisplayItem.

    // The new item cannot be cached, because it is a mutation of the
    // DisplayItem the client thought it was painting.
    paintController.beginSkippingCache();
    {
      // Replay the new SKPicture into a new DrawingDisplayItem in the original
      // DisplayItemList.
      DrawingRecorder newRecorder(graphicsContext, displayItemClient,
                                  displayItemType, cullRect);
      pictureBuilder.endRecording()->playback(graphicsContext.canvas());
    }
    paintController.endSkippingCache();
  } else {
    graphicsContext.getPaintController().endItem<EndCompositingDisplayItem>(
        client);
  }
}

}  // namespace blink
