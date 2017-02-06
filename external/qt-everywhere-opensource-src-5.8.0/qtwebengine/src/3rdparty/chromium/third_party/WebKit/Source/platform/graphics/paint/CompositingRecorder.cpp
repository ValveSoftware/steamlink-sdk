// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/CompositingRecorder.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/CompositingDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"

namespace blink {

CompositingRecorder::CompositingRecorder(GraphicsContext& graphicsContext, const DisplayItemClient& client, const SkXfermode::Mode xferMode, const float opacity, const FloatRect* bounds, ColorFilter colorFilter)
    : m_client(client)
    , m_graphicsContext(graphicsContext)
{
    beginCompositing(graphicsContext, m_client, xferMode, opacity, bounds, colorFilter);
}

CompositingRecorder::~CompositingRecorder()
{
    endCompositing(m_graphicsContext, m_client);
}

void CompositingRecorder::beginCompositing(GraphicsContext& graphicsContext, const DisplayItemClient& client, const SkXfermode::Mode xferMode, const float opacity, const FloatRect* bounds, ColorFilter colorFilter)
{
    graphicsContext.getPaintController().createAndAppend<BeginCompositingDisplayItem>(client, xferMode, opacity, bounds, colorFilter);
}

void CompositingRecorder::endCompositing(GraphicsContext& graphicsContext, const DisplayItemClient& client)
{
    graphicsContext.getPaintController().endItem<EndCompositingDisplayItem>(client);
}

} // namespace blink
