// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CanvasSurfaceLayerBridgeClientImpl.h"

#include "public/platform/Platform.h"
#include "public/platform/ServiceRegistry.h"

namespace blink {

CanvasSurfaceLayerBridgeClientImpl::CanvasSurfaceLayerBridgeClientImpl()
{
    DCHECK(!m_service.is_bound());
    Platform::current()->serviceRegistry()->connectToRemoteService(mojo::GetProxy(&m_service));
}

CanvasSurfaceLayerBridgeClientImpl::~CanvasSurfaceLayerBridgeClientImpl()
{
}

bool CanvasSurfaceLayerBridgeClientImpl::syncGetSurfaceId(cc::SurfaceId* surfaceIdPtr)
{
    return m_service->GetSurfaceId(surfaceIdPtr);
}

void CanvasSurfaceLayerBridgeClientImpl::asyncRequestSurfaceCreation(const cc::SurfaceId& surfaceId)
{
    m_service->RequestSurfaceCreation(surfaceId);
}

void CanvasSurfaceLayerBridgeClientImpl::asyncRequire(const cc::SurfaceId& surfaceId, const cc::SurfaceSequence& sequence)
{
    m_service->Require(surfaceId, sequence);
}

void CanvasSurfaceLayerBridgeClientImpl::asyncSatisfy(const cc::SurfaceSequence& sequence)
{
    m_service->Satisfy(sequence);
}

} // namespace blink;
