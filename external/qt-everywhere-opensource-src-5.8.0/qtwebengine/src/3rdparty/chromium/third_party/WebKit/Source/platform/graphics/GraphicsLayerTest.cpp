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

#include "platform/graphics/GraphicsLayer.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/animation/CompositorAnimation.h"
#include "platform/animation/CompositorAnimationPlayer.h"
#include "platform/animation/CompositorAnimationPlayerClient.h"
#include "platform/animation/CompositorAnimationTimeline.h"
#include "platform/animation/CompositorFloatAnimationCurve.h"
#include "platform/animation/CompositorTargetProperty.h"
#include "platform/graphics/CompositorElementId.h"
#include "platform/scroll/ScrollableArea.h"
#include "platform/testing/FakeGraphicsLayer.h"
#include "platform/testing/FakeGraphicsLayerClient.h"
#include "platform/testing/WebLayerTreeViewImplForTesting.h"
#include "platform/transforms/Matrix3DTransformOperation.h"
#include "platform/transforms/RotateTransformOperation.h"
#include "platform/transforms/TranslateTransformOperation.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebLayerTreeView.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class GraphicsLayerTest : public testing::Test {
public:
    GraphicsLayerTest()
    {
        m_clipLayer = wrapUnique(new FakeGraphicsLayer(&m_client));
        m_scrollElasticityLayer = wrapUnique(new FakeGraphicsLayer(&m_client));
        m_graphicsLayer = wrapUnique(new FakeGraphicsLayer(&m_client));
        m_clipLayer->addChild(m_scrollElasticityLayer.get());
        m_scrollElasticityLayer->addChild(m_graphicsLayer.get());
        m_graphicsLayer->platformLayer()->setScrollClipLayer(
            m_clipLayer->platformLayer());
        m_platformLayer = m_graphicsLayer->platformLayer();
        m_layerTreeView = wrapUnique(new WebLayerTreeViewImplForTesting);
        ASSERT(m_layerTreeView);
        m_layerTreeView->setRootLayer(*m_clipLayer->platformLayer());
        m_layerTreeView->registerViewportLayers(
            m_scrollElasticityLayer->platformLayer(), m_clipLayer->platformLayer(), m_graphicsLayer->platformLayer(), 0);
        m_layerTreeView->setViewportSize(WebSize(1, 1));
    }

    ~GraphicsLayerTest() override
    {
        m_graphicsLayer.reset();
        m_layerTreeView.reset();
    }

    WebLayerTreeView* layerTreeView() { return m_layerTreeView.get(); }

protected:
    WebLayer* m_platformLayer;
    std::unique_ptr<FakeGraphicsLayer> m_graphicsLayer;
    std::unique_ptr<FakeGraphicsLayer> m_scrollElasticityLayer;
    std::unique_ptr<FakeGraphicsLayer> m_clipLayer;

private:
    std::unique_ptr<WebLayerTreeView> m_layerTreeView;
    FakeGraphicsLayerClient m_client;
};

class AnimationPlayerForTesting : public CompositorAnimationPlayerClient {
public:
    AnimationPlayerForTesting()
    {
        m_compositorPlayer = CompositorAnimationPlayer::create();
    }

    CompositorAnimationPlayer* compositorPlayer() const override
    {
        return m_compositorPlayer.get();
    }

    std::unique_ptr<CompositorAnimationPlayer> m_compositorPlayer;
};

TEST_F(GraphicsLayerTest, updateLayerShouldFlattenTransformWithAnimations)
{
    ASSERT_FALSE(m_platformLayer->hasActiveAnimationForTesting());

    std::unique_ptr<CompositorFloatAnimationCurve> curve = CompositorFloatAnimationCurve::create();
    curve->addCubicBezierKeyframe(CompositorFloatKeyframe(0.0, 0.0), CubicBezierTimingFunction::EaseType::EASE);
    std::unique_ptr<CompositorAnimation> floatAnimation(CompositorAnimation::create(*curve, CompositorTargetProperty::OPACITY, 0, 0));
    int animationId = floatAnimation->id();

    std::unique_ptr<CompositorAnimationTimeline> compositorTimeline = CompositorAnimationTimeline::create();
    AnimationPlayerForTesting player;

    layerTreeView()->attachCompositorAnimationTimeline(compositorTimeline->animationTimeline());
    compositorTimeline->playerAttached(player);

    m_platformLayer->setElementId(CompositorElementId(m_platformLayer->id(), 0));

    player.compositorPlayer()->attachElement(m_platformLayer->elementId());
    ASSERT_TRUE(player.compositorPlayer()->isElementAttached());

    player.compositorPlayer()->addAnimation(floatAnimation.release());

    ASSERT_TRUE(m_platformLayer->hasActiveAnimationForTesting());

    m_graphicsLayer->setShouldFlattenTransform(false);

    m_platformLayer = m_graphicsLayer->platformLayer();
    ASSERT_TRUE(m_platformLayer);

    ASSERT_TRUE(m_platformLayer->hasActiveAnimationForTesting());
    player.compositorPlayer()->removeAnimation(animationId);
    ASSERT_FALSE(m_platformLayer->hasActiveAnimationForTesting());

    m_graphicsLayer->setShouldFlattenTransform(true);

    m_platformLayer = m_graphicsLayer->platformLayer();
    ASSERT_TRUE(m_platformLayer);

    ASSERT_FALSE(m_platformLayer->hasActiveAnimationForTesting());

    player.compositorPlayer()->detachElement();
    ASSERT_FALSE(player.compositorPlayer()->isElementAttached());

    compositorTimeline->playerDestroyed(player);
    layerTreeView()->detachCompositorAnimationTimeline(compositorTimeline->animationTimeline());
}

class FakeScrollableArea : public GarbageCollectedFinalized<FakeScrollableArea>, public ScrollableArea {
    USING_GARBAGE_COLLECTED_MIXIN(FakeScrollableArea);
public:
    static FakeScrollableArea* create()
    {
        return new FakeScrollableArea;
    }

    LayoutRect visualRectForScrollbarParts() const override { return LayoutRect(); }
    bool isActive() const override { return false; }
    int scrollSize(ScrollbarOrientation) const override { return 100; }
    bool isScrollCornerVisible() const override { return false; }
    IntRect scrollCornerRect() const override { return IntRect(); }
    int visibleWidth() const override { return 10; }
    int visibleHeight() const override { return 10; }
    IntSize contentsSize() const override { return IntSize(100, 100); }
    bool scrollbarsCanBeActive() const override { return false; }
    IntRect scrollableAreaBoundingBox() const override { return IntRect(); }
    void scrollControlWasSetNeedsPaintInvalidation() override { }
    bool userInputScrollable(ScrollbarOrientation) const override { return true; }
    bool shouldPlaceVerticalScrollbarOnLeft() const override { return false; }
    int pageStep(ScrollbarOrientation) const override { return 0; }
    IntPoint minimumScrollPosition() const override { return IntPoint(); }
    IntPoint maximumScrollPosition() const override
    {
        return IntPoint(contentsSize().width() - visibleWidth(), contentsSize().height() - visibleHeight());
    }

    void setScrollOffset(const DoublePoint& scrollOffset, ScrollType) override { m_scrollPosition = scrollOffset; }
    DoublePoint scrollPositionDouble() const override { return m_scrollPosition; }
    IntPoint scrollPosition() const override { return flooredIntPoint(m_scrollPosition); }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        ScrollableArea::trace(visitor);
    }

private:
    DoublePoint m_scrollPosition;
};

TEST_F(GraphicsLayerTest, applyScrollToScrollableArea)
{
    FakeScrollableArea* scrollableArea = FakeScrollableArea::create();
    m_graphicsLayer->setScrollableArea(scrollableArea, false);

    WebDoublePoint scrollPosition(7, 9);
    m_platformLayer->setScrollPositionDouble(scrollPosition);
    m_graphicsLayer->didScroll();

    EXPECT_FLOAT_EQ(scrollPosition.x, scrollableArea->scrollPositionDouble().x());
    EXPECT_FLOAT_EQ(scrollPosition.y, scrollableArea->scrollPositionDouble().y());
}

} // namespace blink
