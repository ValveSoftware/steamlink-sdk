// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasSurfaceLayerBridge_h
#define CanvasSurfaceLayerBridge_h

#include "base/memory/ref_counted.h"
#include "cc/surfaces/surface_id.h"
#include "platform/PlatformExport.h"
#include "platform/graphics/CanvasSurfaceLayerBridgeClient.h"
#include <memory>

namespace cc {
class SurfaceLayer;
struct SurfaceSequence;
} // namespace cc

namespace blink {

class WebLayer;

class PLATFORM_EXPORT CanvasSurfaceLayerBridge {
public:
    explicit CanvasSurfaceLayerBridge(std::unique_ptr<CanvasSurfaceLayerBridgeClient>);
    ~CanvasSurfaceLayerBridge();
    bool createSurfaceLayer(int canvasWidth, int canvasHeight);
    WebLayer* getWebLayer() const { return m_webLayer.get(); }
    const cc::SurfaceId& getSurfaceId() const { return m_surfaceId; }
    CanvasSurfaceLayerBridgeClient* getClient() const { return m_client.get(); }

    void satisfyCallback(const cc::SurfaceSequence&);
    void requireCallback(const cc::SurfaceId&, const cc::SurfaceSequence&);

private:
    scoped_refptr<cc::SurfaceLayer> m_surfaceLayer;
    std::unique_ptr<WebLayer> m_webLayer;
    std::unique_ptr<CanvasSurfaceLayerBridgeClient> m_client;
    cc::SurfaceId m_surfaceId;
};

} // namespace blink

#endif // CanvasSurfaceLayerBridge_h
