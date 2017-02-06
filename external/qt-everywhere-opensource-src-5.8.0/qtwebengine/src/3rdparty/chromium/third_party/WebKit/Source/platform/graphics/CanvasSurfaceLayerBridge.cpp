// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CanvasSurfaceLayerBridge.h"

#include "cc/layers/surface_layer.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_sequence.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebLayer.h"
#include "ui/gfx/geometry/size.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"

namespace blink {

CanvasSurfaceLayerBridge::CanvasSurfaceLayerBridge(std::unique_ptr<CanvasSurfaceLayerBridgeClient> client)
{
    m_client = std::move(client);
}

CanvasSurfaceLayerBridge::~CanvasSurfaceLayerBridge()
{
}

bool CanvasSurfaceLayerBridge::createSurfaceLayer(int canvasWidth, int canvasHeight)
{
    if (!m_client->syncGetSurfaceId(&m_surfaceId))
        return false;

    m_client->asyncRequestSurfaceCreation(m_surfaceId);
    cc::SurfaceLayer::SatisfyCallback satisfyCallback = createBaseCallback(WTF::bind(&CanvasSurfaceLayerBridge::satisfyCallback, WTF::unretained(this)));
    cc::SurfaceLayer::RequireCallback requireCallback = createBaseCallback(WTF::bind(&CanvasSurfaceLayerBridge::requireCallback, WTF::unretained(this)));
    m_surfaceLayer = cc::SurfaceLayer::Create(std::move(satisfyCallback), std::move(requireCallback));
    m_surfaceLayer->SetSurfaceId(m_surfaceId, 1.f, gfx::Size(canvasWidth, canvasHeight));

    m_webLayer = wrapUnique(Platform::current()->compositorSupport()->createLayerFromCCLayer(m_surfaceLayer.get()));
    GraphicsLayer::registerContentsLayer(m_webLayer.get());
    return true;
}

void CanvasSurfaceLayerBridge::satisfyCallback(const cc::SurfaceSequence& sequence)
{
    m_client->asyncSatisfy(sequence);
}

void CanvasSurfaceLayerBridge::requireCallback(const cc::SurfaceId& surfaceId, const cc::SurfaceSequence& sequence)
{
    m_client->asyncRequire(surfaceId, sequence);
}

} // namespace blink
