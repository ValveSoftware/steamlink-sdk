// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/HTMLCanvasPainter.h"

#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/loader/EmptyClients.h"
#include "core/paint/StubChromeClientForSPv2.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/graphics/Canvas2DImageBufferSurface.h"
#include "platform/graphics/Canvas2DLayerBridge.h"
#include "platform/graphics/test/FakeGLES2Interface.h"
#include "platform/graphics/test/FakeWebGraphicsContext3DProvider.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebSize.h"
#include "testing/gtest/include/gtest/gtest.h"

// Integration tests of canvas painting code (in SPv2 mode).

namespace blink {

class HTMLCanvasPainterTestForSPv2 : public ::testing::Test,
                                     public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(true);
    RuntimeEnabledFeatures::setRootLayerScrollingEnabled(GetParam());
    m_chromeClient = new StubChromeClientForSPv2();
    Page::PageClients clients;
    fillWithEmptyClients(clients);
    clients.chromeClient = m_chromeClient.get();
    m_pageHolder = DummyPageHolder::create(
        IntSize(800, 600), &clients, nullptr, [](Settings& settings) {
          settings.setAcceleratedCompositingEnabled(true);
          // LayoutHTMLCanvas doesn't exist if script is disabled.
          settings.setScriptEnabled(true);
        });
    document().view()->setParentVisible(true);
    document().view()->setSelfVisible(true);
  }

  void TearDown() override { m_featuresBackup.restore(); }

  Document& document() { return m_pageHolder->document(); }
  bool hasLayerAttached(const WebLayer& layer) {
    return m_chromeClient->hasLayer(layer);
  }

  PassRefPtr<Canvas2DLayerBridge> makeCanvas2DLayerBridge(const IntSize& size) {
    return adoptRef(new Canvas2DLayerBridge(
        wrapUnique(new FakeWebGraphicsContext3DProvider(&m_gl)), size, 0,
        NonOpaque, Canvas2DLayerBridge::ForceAccelerationForTesting, nullptr,
        kN32_SkColorType));
  }

 private:
  RuntimeEnabledFeatures::Backup m_featuresBackup;
  Persistent<StubChromeClientForSPv2> m_chromeClient;
  FakeGLES2Interface m_gl;
  std::unique_ptr<DummyPageHolder> m_pageHolder;
};

INSTANTIATE_TEST_CASE_P(All, HTMLCanvasPainterTestForSPv2, ::testing::Bool());

TEST_P(HTMLCanvasPainterTestForSPv2, Canvas2DLayerAppearsInLayerTree) {
  // Insert a <canvas> and force it into accelerated mode.
  document().body()->setInnerHTML("<canvas width=300 height=200>");
  HTMLCanvasElement* element =
      toHTMLCanvasElement(document().body()->firstChild());
  CanvasContextCreationAttributes attributes;
  attributes.setAlpha(true);
  CanvasRenderingContext* context =
      element->getCanvasRenderingContext("2d", attributes);
  RefPtr<Canvas2DLayerBridge> bridge =
      makeCanvas2DLayerBridge(IntSize(300, 200));
  element->createImageBufferUsingSurfaceForTesting(
      wrapUnique(new Canvas2DImageBufferSurface(bridge, IntSize(300, 200))));
  ASSERT_EQ(context, element->renderingContext());
  ASSERT_TRUE(context->isAccelerated());

  // Force the page to paint.
  document().view()->updateAllLifecyclePhases();

  // Fetch the layer associated with the <canvas>, and check that it was
  // correctly configured in the layer tree.
  const WebLayer* layer = context->platformLayer();
  ASSERT_TRUE(layer);
  EXPECT_TRUE(hasLayerAttached(*layer));
  EXPECT_EQ(WebSize(300, 200), layer->bounds());
}

}  // namespace blink
