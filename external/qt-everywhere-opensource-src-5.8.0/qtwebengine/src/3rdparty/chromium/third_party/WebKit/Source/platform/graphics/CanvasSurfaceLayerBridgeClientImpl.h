// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasSurfaceLayerBridgeClientImpl_h
#define CanvasSurfaceLayerBridgeClientImpl_h

#include "platform/graphics/CanvasSurfaceLayerBridgeClient.h"
#include "public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom-blink.h"
#include "wtf/RefPtr.h"

namespace blink {

class PLATFORM_EXPORT CanvasSurfaceLayerBridgeClientImpl final : public CanvasSurfaceLayerBridgeClient {
public:
    explicit CanvasSurfaceLayerBridgeClientImpl();
    ~CanvasSurfaceLayerBridgeClientImpl() override;

    bool syncGetSurfaceId(cc::SurfaceId*) override;
    void asyncRequestSurfaceCreation(const cc::SurfaceId&) override;
    void asyncRequire(const cc::SurfaceId&, const cc::SurfaceSequence&) override;
    void asyncSatisfy(const cc::SurfaceSequence&) override;

private:
    mojom::blink::OffscreenCanvasSurfacePtr m_service;
};

} // namespace blink

#endif // CanvasSurfaceLayerBridgeClientImpl_h
