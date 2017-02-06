// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CanvasSurfaceLayerBridge.h"

#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_sequence.h"
#include "platform/graphics/CanvasSurfaceLayerBridgeClient.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class FakeOffscreenCanvasSurfaceImpl {
public:
    FakeOffscreenCanvasSurfaceImpl() {}
    ~FakeOffscreenCanvasSurfaceImpl();

    bool GetSurfaceId(cc::SurfaceId*);
    void RequestSurfaceCreation(const cc::SurfaceId&);

    bool isSurfaceInSurfaceMap(const cc::SurfaceId&);

private:
    Vector<cc::SurfaceId> m_fakeSurfaceMap;
};

//-----------------------------------------------------------------------------

class MockCanvasSurfaceLayerBridgeClient final : public CanvasSurfaceLayerBridgeClient {
public:
    explicit MockCanvasSurfaceLayerBridgeClient(FakeOffscreenCanvasSurfaceImpl*);
    ~MockCanvasSurfaceLayerBridgeClient() override;

    bool syncGetSurfaceId(cc::SurfaceId*) override;
    void asyncRequestSurfaceCreation(const cc::SurfaceId&) override;
    void asyncRequire(const cc::SurfaceId&, const cc::SurfaceSequence&) override {}
    void asyncSatisfy(const cc::SurfaceSequence&) override {}

private:
    FakeOffscreenCanvasSurfaceImpl* m_service;
    cc::SurfaceId m_surfaceId;
};

//-----------------------------------------------------------------------------

class CanvasSurfaceLayerBridgeTest : public testing::Test {
public:
    CanvasSurfaceLayerBridge* surfaceLayerBridge() const { return m_surfaceLayerBridge.get(); }
    FakeOffscreenCanvasSurfaceImpl* surfaceService() const { return m_surfaceService.get(); }

protected:
    void SetUp() override;

private:
    std::unique_ptr<FakeOffscreenCanvasSurfaceImpl> m_surfaceService;
    std::unique_ptr<CanvasSurfaceLayerBridge> m_surfaceLayerBridge;
};

//-----------------------------------------------------------------------------

MockCanvasSurfaceLayerBridgeClient::MockCanvasSurfaceLayerBridgeClient(FakeOffscreenCanvasSurfaceImpl* surfaceService)
{
    m_service = surfaceService;
}

MockCanvasSurfaceLayerBridgeClient::~MockCanvasSurfaceLayerBridgeClient()
{
}

bool MockCanvasSurfaceLayerBridgeClient::syncGetSurfaceId(cc::SurfaceId* surfaceIdPtr)
{
    return m_service->GetSurfaceId(surfaceIdPtr);
}

void MockCanvasSurfaceLayerBridgeClient::asyncRequestSurfaceCreation(const cc::SurfaceId& surfaceId)
{
    m_service->RequestSurfaceCreation(surfaceId);
}

FakeOffscreenCanvasSurfaceImpl::~FakeOffscreenCanvasSurfaceImpl()
{
    m_fakeSurfaceMap.clear();
}

bool FakeOffscreenCanvasSurfaceImpl::GetSurfaceId(cc::SurfaceId* surfaceId)
{
    *surfaceId = cc::SurfaceId(10, 15, 0);
    return true;
}

void FakeOffscreenCanvasSurfaceImpl::RequestSurfaceCreation(const cc::SurfaceId& surfaceId)
{
    m_fakeSurfaceMap.append(surfaceId);
}

bool FakeOffscreenCanvasSurfaceImpl::isSurfaceInSurfaceMap(const cc::SurfaceId& surfaceId)
{
    return m_fakeSurfaceMap.contains(surfaceId);
}

void CanvasSurfaceLayerBridgeTest::SetUp()
{
    m_surfaceService = wrapUnique(new FakeOffscreenCanvasSurfaceImpl());
    std::unique_ptr<CanvasSurfaceLayerBridgeClient> bridgeClient = wrapUnique(new MockCanvasSurfaceLayerBridgeClient(m_surfaceService.get()));
    m_surfaceLayerBridge = wrapUnique(new CanvasSurfaceLayerBridge(std::move(bridgeClient)));
}

TEST_F(CanvasSurfaceLayerBridgeTest, SurfaceLayerCreation)
{
    bool success = this->surfaceLayerBridge()->createSurfaceLayer(50, 50);
    EXPECT_TRUE(this->surfaceService()->isSurfaceInSurfaceMap(this->surfaceLayerBridge()->getSurfaceId()));
    EXPECT_TRUE(success);
}

} // namespace blink
