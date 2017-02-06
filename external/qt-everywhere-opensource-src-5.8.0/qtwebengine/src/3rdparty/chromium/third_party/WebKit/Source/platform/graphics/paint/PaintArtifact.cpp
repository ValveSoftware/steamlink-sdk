// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintArtifact.h"

#include "platform/TraceEvent.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "public/platform/WebDisplayItemList.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace blink {

namespace {

void computeChunkBoundsAndOpaqueness(const DisplayItemList& displayItems, Vector<PaintChunk>& paintChunks)
{
    for (PaintChunk& chunk : paintChunks) {
        FloatRect bounds;
        SkRegion knownToBeOpaqueRegion;
        for (const DisplayItem& item : displayItems.itemsInPaintChunk(chunk)) {
            if (!item.isDrawing())
                continue;
            const auto& drawing = static_cast<const DrawingDisplayItem&>(item);
            if (const SkPicture* picture = drawing.picture()) {
                const SkRect& pictureRect = picture->cullRect();
                bounds.unite(pictureRect);
                if (drawing.knownToBeOpaque()) {
                    // TODO(pdr): This may be too conservative and fail due to
                    // floating point precision issues.
                    SkIRect conservativelyRoundedPictureRect;
                    pictureRect.roundIn(&conservativelyRoundedPictureRect);
                    knownToBeOpaqueRegion.op(conservativelyRoundedPictureRect, SkRegion::kUnion_Op);
                }
            }
        }
        chunk.bounds = bounds;
        if (knownToBeOpaqueRegion.contains(enclosingIntRect(bounds)))
            chunk.knownToBeOpaque = true;
    }
}

} // namespace

PaintArtifact::PaintArtifact()
    : m_displayItemList(0)
    , m_isSuitableForGpuRasterization(true)
{
}

PaintArtifact::PaintArtifact(DisplayItemList displayItems, Vector<PaintChunk> paintChunks, bool isSuitableForGpuRasterizationArg)
    : m_displayItemList(std::move(displayItems))
    , m_paintChunks(std::move(paintChunks))
    , m_isSuitableForGpuRasterization(isSuitableForGpuRasterizationArg)
{
    computeChunkBoundsAndOpaqueness(m_displayItemList, m_paintChunks);
}

PaintArtifact::PaintArtifact(PaintArtifact&& source)
    : m_displayItemList(std::move(source.m_displayItemList))
    , m_paintChunks(std::move(source.m_paintChunks))
    , m_isSuitableForGpuRasterization(source.m_isSuitableForGpuRasterization)
{
}

PaintArtifact::~PaintArtifact()
{
}

PaintArtifact& PaintArtifact::operator=(PaintArtifact&& source)
{
    m_displayItemList = std::move(source.m_displayItemList);
    m_paintChunks = std::move(source.m_paintChunks);
    m_isSuitableForGpuRasterization = source.m_isSuitableForGpuRasterization;
    return *this;
}

void PaintArtifact::reset()
{
    m_displayItemList.clear();
    m_paintChunks.clear();
}

size_t PaintArtifact::approximateUnsharedMemoryUsage() const
{
    return sizeof(*this) + m_displayItemList.memoryUsageInBytes()
        + m_paintChunks.capacity() * sizeof(m_paintChunks[0]);
}

void PaintArtifact::replay(GraphicsContext& graphicsContext) const
{
    TRACE_EVENT0("blink,benchmark", "PaintArtifact::replay");
    for (const DisplayItem& displayItem : m_displayItemList)
        displayItem.replay(graphicsContext);
}

void PaintArtifact::appendToWebDisplayItemList(WebDisplayItemList* list) const
{
    TRACE_EVENT0("blink,benchmark", "PaintArtifact::appendToWebDisplayItemList");
    unsigned visualRectIndex = 0;
    for (const DisplayItem& displayItem : m_displayItemList) {
        displayItem.appendToWebDisplayItemList(m_displayItemList.visualRect(visualRectIndex), list);
        visualRectIndex++;
    }
    list->setIsSuitableForGpuRasterization(isSuitableForGpuRasterization());
}

} // namespace blink
