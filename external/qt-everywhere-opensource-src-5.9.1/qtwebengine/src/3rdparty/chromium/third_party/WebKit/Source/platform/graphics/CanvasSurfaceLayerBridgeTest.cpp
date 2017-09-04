// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CanvasSurfaceLayerBridge.h"

#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_sequence.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

//-----------------------------------------------------------------------------

class MockOffscreenCanvasSurface final
    : public mojom::blink::OffscreenCanvasSurface {
 public:
  MockOffscreenCanvasSurface();
  ~MockOffscreenCanvasSurface() override;

  mojom::blink::OffscreenCanvasSurfacePtr GetProxy();

  void GetSurfaceId(const GetSurfaceIdCallback&) override;
  void Require(const cc::SurfaceId&, const cc::SurfaceSequence&) override {}
  void Satisfy(const cc::SurfaceSequence&) override {}

 private:
  mojo::Binding<mojom::blink::OffscreenCanvasSurface> m_binding;
  cc::SurfaceId m_surfaceId;
};

//-----------------------------------------------------------------------------

class CanvasSurfaceLayerBridgeTest : public testing::Test {
 public:
  CanvasSurfaceLayerBridge* surfaceLayerBridge() const {
    return m_surfaceLayerBridge.get();
  }

 protected:
  void SetUp() override;

 private:
  MockOffscreenCanvasSurface m_service;
  std::unique_ptr<CanvasSurfaceLayerBridge> m_surfaceLayerBridge;
};

//-----------------------------------------------------------------------------

MockOffscreenCanvasSurface::MockOffscreenCanvasSurface() : m_binding(this) {}

MockOffscreenCanvasSurface::~MockOffscreenCanvasSurface() {}

mojom::blink::OffscreenCanvasSurfacePtr MockOffscreenCanvasSurface::GetProxy() {
  return m_binding.CreateInterfacePtrAndBind();
}

void MockOffscreenCanvasSurface::GetSurfaceId(
    const GetSurfaceIdCallback& callback) {
  callback.Run(
      cc::SurfaceId(cc::FrameSinkId(10, 11),
                    cc::LocalFrameId(15, base::UnguessableToken::Create())));
}

void CanvasSurfaceLayerBridgeTest::SetUp() {
  m_surfaceLayerBridge =
      wrapUnique(new CanvasSurfaceLayerBridge(m_service.GetProxy()));
}

TEST_F(CanvasSurfaceLayerBridgeTest, SurfaceLayerCreation) {
  bool success = this->surfaceLayerBridge()->createSurfaceLayer(50, 50);
  EXPECT_TRUE(success);
}

}  // namespace blink
