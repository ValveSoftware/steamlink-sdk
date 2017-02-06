// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/TestPaintArtifact.h"

#include "cc/layers/layer.h"
#include "platform/graphics/paint/DisplayItemClient.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/ForeignLayerDisplayItem.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "wtf/Assertions.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class TestPaintArtifact::DummyRectClient : public DisplayItemClient {
    USING_FAST_MALLOC(DummyRectClient);
public:
    DummyRectClient(const FloatRect& rect, Color color) : m_rect(rect), m_color(color) {}
    String debugName() const final { return "<dummy>"; }
    LayoutRect visualRect() const final { return enclosingLayoutRect(m_rect); }
    PassRefPtr<SkPicture> makePicture() const;
private:
    FloatRect m_rect;
    Color m_color;
};

PassRefPtr<SkPicture> TestPaintArtifact::DummyRectClient::makePicture() const
{
    SkPictureRecorder recorder;
    SkCanvas* canvas = recorder.beginRecording(m_rect);
    SkPaint paint;
    paint.setColor(m_color.rgb());
    canvas->drawRect(m_rect, paint);
    return fromSkSp(recorder.finishRecordingAsPicture());
}

TestPaintArtifact::TestPaintArtifact()
    : m_displayItemList(0), m_built(false)
{
}

TestPaintArtifact::~TestPaintArtifact()
{
}

TestPaintArtifact& TestPaintArtifact::chunk(
    PassRefPtr<TransformPaintPropertyNode> transform,
    PassRefPtr<ClipPaintPropertyNode> clip,
    PassRefPtr<EffectPaintPropertyNode> effect)
{
    PaintChunkProperties properties;
    properties.transform = transform;
    properties.clip = clip;
    properties.effect = effect;
    return chunk(properties);
}

TestPaintArtifact& TestPaintArtifact::chunk(const PaintChunkProperties& properties)
{
    if (!m_paintChunks.isEmpty())
        m_paintChunks.last().endIndex = m_displayItemList.size();
    PaintChunk chunk;
    chunk.beginIndex = m_displayItemList.size();
    chunk.properties = properties;
    m_paintChunks.append(chunk);
    return *this;
}

TestPaintArtifact& TestPaintArtifact::rectDrawing(const FloatRect& bounds, Color color)
{
    std::unique_ptr<DummyRectClient> client = wrapUnique(new DummyRectClient(bounds, color));
    m_displayItemList.allocateAndConstruct<DrawingDisplayItem>(
        *client, DisplayItem::DrawingFirst, client->makePicture());
    m_dummyClients.append(std::move(client));
    return *this;
}

TestPaintArtifact& TestPaintArtifact::foreignLayer(const FloatPoint& location, const IntSize& size, scoped_refptr<cc::Layer> layer)
{
    FloatRect floatBounds(location, FloatSize(size));
    std::unique_ptr<DummyRectClient> client = wrapUnique(new DummyRectClient(floatBounds, Color::transparent));
    m_displayItemList.allocateAndConstruct<ForeignLayerDisplayItem>(
        *client, DisplayItem::ForeignLayerFirst, std::move(layer), location, size);
    m_dummyClients.append(std::move(client));
    return *this;
}

const PaintArtifact& TestPaintArtifact::build()
{
    if (m_built)
        return m_paintArtifact;

    if (!m_paintChunks.isEmpty())
        m_paintChunks.last().endIndex = m_displayItemList.size();
    m_paintArtifact = PaintArtifact(std::move(m_displayItemList), std::move(m_paintChunks), true);
    m_built = true;
    return m_paintArtifact;
}

} // namespace blink
