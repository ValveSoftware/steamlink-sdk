/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "platform/graphics/GraphicsLayer.h"

#include "platform/scroll/ScrollableArea.h"
#include "platform/transforms/Matrix3DTransformOperation.h"
#include "platform/transforms/RotateTransformOperation.h"
#include "platform/transforms/TranslateTransformOperation.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebFloatAnimationCurve.h"
#include "public/platform/WebGraphicsContext3D.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebUnitTestSupport.h"
#include "wtf/PassOwnPtr.h"

#include <gtest/gtest.h>

using namespace WebCore;
using namespace blink;

namespace {

class MockGraphicsLayerClient : public GraphicsLayerClient {
public:
    virtual void notifyAnimationStarted(const GraphicsLayer*, double monotonicTime) OVERRIDE { }
    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect& inClip) OVERRIDE { }
    virtual String debugName(const GraphicsLayer*) OVERRIDE { return String(); }
};

class GraphicsLayerForTesting : public GraphicsLayer {
public:
    explicit GraphicsLayerForTesting(GraphicsLayerClient* client)
        : GraphicsLayer(client) { };

    virtual blink::WebLayer* contentsLayer() const { return GraphicsLayer::contentsLayer(); }
};

class GraphicsLayerTest : public testing::Test {
public:
    GraphicsLayerTest()
    {
        m_clipLayer = adoptPtr(new GraphicsLayerForTesting(&m_client));
        m_graphicsLayer = adoptPtr(new GraphicsLayerForTesting(&m_client));
        m_clipLayer->addChild(m_graphicsLayer.get());
        m_graphicsLayer->platformLayer()->setScrollClipLayer(
            m_clipLayer->platformLayer());
        m_platformLayer = m_graphicsLayer->platformLayer();
        m_layerTreeView = adoptPtr(Platform::current()->unitTestSupport()->createLayerTreeViewForTesting());
        ASSERT(m_layerTreeView);
        m_layerTreeView->setRootLayer(*m_clipLayer->platformLayer());
        m_layerTreeView->registerViewportLayers(
            m_clipLayer->platformLayer(), m_graphicsLayer->platformLayer(), 0);
        m_layerTreeView->setViewportSize(WebSize(1, 1));
    }

    virtual ~GraphicsLayerTest()
    {
        m_graphicsLayer.clear();
        m_layerTreeView.clear();
    }

protected:
    WebLayer* m_platformLayer;
    OwnPtr<GraphicsLayerForTesting> m_graphicsLayer;
    OwnPtr<GraphicsLayerForTesting> m_clipLayer;

private:
    OwnPtr<WebLayerTreeView> m_layerTreeView;
    MockGraphicsLayerClient m_client;
};

TEST_F(GraphicsLayerTest, updateLayerShouldFlattenTransformWithAnimations)
{
    ASSERT_FALSE(m_platformLayer->hasActiveAnimation());

    OwnPtr<WebFloatAnimationCurve> curve = adoptPtr(Platform::current()->compositorSupport()->createFloatAnimationCurve());
    curve->add(WebFloatKeyframe(0.0, 0.0));
    OwnPtr<WebAnimation> floatAnimation(adoptPtr(Platform::current()->compositorSupport()->createAnimation(*curve, WebAnimation::TargetPropertyOpacity)));
    int animationId = floatAnimation->id();
    ASSERT_TRUE(m_platformLayer->addAnimation(floatAnimation.leakPtr()));

    ASSERT_TRUE(m_platformLayer->hasActiveAnimation());

    m_graphicsLayer->setShouldFlattenTransform(false);

    m_platformLayer = m_graphicsLayer->platformLayer();
    ASSERT_TRUE(m_platformLayer);

    ASSERT_TRUE(m_platformLayer->hasActiveAnimation());
    m_platformLayer->removeAnimation(animationId);
    ASSERT_FALSE(m_platformLayer->hasActiveAnimation());

    m_graphicsLayer->setShouldFlattenTransform(true);

    m_platformLayer = m_graphicsLayer->platformLayer();
    ASSERT_TRUE(m_platformLayer);

    ASSERT_FALSE(m_platformLayer->hasActiveAnimation());
}

class FakeScrollableArea : public ScrollableArea {
public:
    virtual bool isActive() const OVERRIDE { return false; }
    virtual int scrollSize(ScrollbarOrientation) const OVERRIDE { return 100; }
    virtual bool isScrollCornerVisible() const OVERRIDE { return false; }
    virtual IntRect scrollCornerRect() const OVERRIDE { return IntRect(); }
    virtual int visibleWidth() const OVERRIDE { return 10; }
    virtual int visibleHeight() const OVERRIDE { return 10; }
    virtual IntSize contentsSize() const OVERRIDE { return IntSize(100, 100); }
    virtual bool scrollbarsCanBeActive() const OVERRIDE { return false; }
    virtual IntRect scrollableAreaBoundingBox() const OVERRIDE { return IntRect(); }
    virtual void invalidateScrollbarRect(Scrollbar*, const IntRect&) OVERRIDE { }
    virtual void invalidateScrollCornerRect(const IntRect&) OVERRIDE { }
    virtual bool userInputScrollable(ScrollbarOrientation) const OVERRIDE { return true; }
    virtual bool shouldPlaceVerticalScrollbarOnLeft() const OVERRIDE { return false; }
    virtual int pageStep(ScrollbarOrientation) const OVERRIDE { return 0; }
    virtual IntPoint minimumScrollPosition() const OVERRIDE { return IntPoint(); }
    virtual IntPoint maximumScrollPosition() const OVERRIDE
    {
        return IntPoint(contentsSize().width() - visibleWidth(), contentsSize().height() - visibleHeight());
    }

    virtual void setScrollOffset(const IntPoint& scrollOffset) OVERRIDE { m_scrollPosition = scrollOffset; }
    virtual IntPoint scrollPosition() const OVERRIDE { return m_scrollPosition; }

private:
    IntPoint m_scrollPosition;
};

TEST_F(GraphicsLayerTest, applyScrollToScrollableArea)
{
    FakeScrollableArea scrollableArea;
    m_graphicsLayer->setScrollableArea(&scrollableArea, false);

    WebPoint scrollPosition(7, 9);
    m_platformLayer->setScrollPosition(scrollPosition);
    m_graphicsLayer->didScroll();

    EXPECT_EQ(scrollPosition, WebPoint(scrollableArea.scrollPosition()));
}

} // namespace
