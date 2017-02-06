// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasSurfaceLayerBridgeClient_h
#define CanvasSurfaceLayerBridgeClient_h

#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_sequence.h"
#include "platform/PlatformExport.h"

namespace blink {

// This class is an interface for all mojo calls from CanvasSurfaceLayerBridge
// to OffscreenCanvasSurfaceService.
class PLATFORM_EXPORT CanvasSurfaceLayerBridgeClient {
public:
    virtual ~CanvasSurfaceLayerBridgeClient() {};

    // Calls that help initial creation of SurfaceLayer.
    virtual bool syncGetSurfaceId(cc::SurfaceId*) = 0;
    virtual void asyncRequestSurfaceCreation(const cc::SurfaceId&) = 0;

    // Calls that ensure correct destruction order of surface.
    virtual void asyncRequire(const cc::SurfaceId&, const cc::SurfaceSequence&) = 0;
    virtual void asyncSatisfy(const cc::SurfaceSequence&) = 0;
};

} // namespace blink

#endif // CanvasSurfaceLayerBridgeClient_h
