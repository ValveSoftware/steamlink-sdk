// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/WebLayerTreeViewImplForTesting.h"

#include "base/threading/thread_task_runner_handle.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/trees/layer_tree_host.h"
#include "public/platform/Platform.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebSize.h"

namespace blink {

WebLayerTreeViewImplForTesting::WebLayerTreeViewImplForTesting()
    : WebLayerTreeViewImplForTesting(defaultLayerTreeSettings())
{
}

WebLayerTreeViewImplForTesting::WebLayerTreeViewImplForTesting(const cc::LayerTreeSettings& settings)
{
    cc::LayerTreeHost::InitParams params;
    params.client = this;
    params.settings = &settings;
    params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
    params.task_graph_runner = &m_taskGraphRunner;
    params.animation_host = cc::AnimationHost::CreateForTesting(cc::ThreadInstance::MAIN);
    m_layerTreeHost = cc::LayerTreeHost::CreateSingleThreaded(this, &params);
    ASSERT(m_layerTreeHost);
}

WebLayerTreeViewImplForTesting::~WebLayerTreeViewImplForTesting() {}

// static
cc::LayerTreeSettings WebLayerTreeViewImplForTesting::defaultLayerTreeSettings()
{
    cc::LayerTreeSettings settings;

    // For web contents, layer transforms should scale up the contents of layers
    // to keep content always crisp when possible.
    settings.layer_transforms_should_scale_layer_contents = true;

    return settings;
}

void WebLayerTreeViewImplForTesting::setRootLayer(const blink::WebLayer& root)
{
    m_layerTreeHost->SetRootLayer(static_cast<const cc_blink::WebLayerImpl*>(&root)->layer());
}

void WebLayerTreeViewImplForTesting::clearRootLayer()
{
    m_layerTreeHost->SetRootLayer(scoped_refptr<cc::Layer>());
}

void WebLayerTreeViewImplForTesting::attachCompositorAnimationTimeline(cc::AnimationTimeline* compositorTimeline)
{
    ASSERT(m_layerTreeHost->animation_host());
    m_layerTreeHost->animation_host()->AddAnimationTimeline(compositorTimeline);
}

void WebLayerTreeViewImplForTesting::detachCompositorAnimationTimeline(cc::AnimationTimeline* compositorTimeline)
{
    ASSERT(m_layerTreeHost->animation_host());
    m_layerTreeHost->animation_host()->RemoveAnimationTimeline(compositorTimeline);
}

void WebLayerTreeViewImplForTesting::setViewportSize(const WebSize& unusedDeprecated, const WebSize& deviceViewportSize)
{
    gfx::Size gfxSize(std::max(0, deviceViewportSize.width), std::max(0, deviceViewportSize.height));
    m_layerTreeHost->SetViewportSize(gfxSize);
}

void WebLayerTreeViewImplForTesting::setViewportSize(const WebSize& deviceViewportSize)
{
    gfx::Size gfxSize(std::max(0, deviceViewportSize.width), std::max(0, deviceViewportSize.height));
    m_layerTreeHost->SetViewportSize(gfxSize);
}

void WebLayerTreeViewImplForTesting::setDeviceScaleFactor(float deviceScaleFactor)
{
    m_layerTreeHost->SetDeviceScaleFactor(deviceScaleFactor);
}

void WebLayerTreeViewImplForTesting::setBackgroundColor(WebColor color)
{
    m_layerTreeHost->set_background_color(color);
}

void WebLayerTreeViewImplForTesting::setHasTransparentBackground(bool transparent)
{
    m_layerTreeHost->set_has_transparent_background(transparent);
}

void WebLayerTreeViewImplForTesting::setVisible(bool visible)
{
    m_layerTreeHost->SetVisible(visible);
}

void WebLayerTreeViewImplForTesting::setPageScaleFactorAndLimits(
    float pageScaleFactor,
    float minimum,
    float maximum)
{
    m_layerTreeHost->SetPageScaleFactorAndLimits(pageScaleFactor, minimum, maximum);
}

void WebLayerTreeViewImplForTesting::startPageScaleAnimation(
    const blink::WebPoint& scroll,
    bool useAnchor,
    float newPageScale,
    double durationSec) {}

void WebLayerTreeViewImplForTesting::setNeedsAnimate()
{
    m_layerTreeHost->SetNeedsAnimate();
}

void WebLayerTreeViewImplForTesting::didStopFlinging() {}

void WebLayerTreeViewImplForTesting::setDeferCommits(bool deferCommits)
{
    m_layerTreeHost->SetDeferCommits(deferCommits);
}

void WebLayerTreeViewImplForTesting::UpdateLayerTreeHost()
{
}

void WebLayerTreeViewImplForTesting::ApplyViewportDeltas(
    const gfx::Vector2dF& innerDelta,
    const gfx::Vector2dF& outerDelta,
    const gfx::Vector2dF& elasticOverscrollDelta,
    float pageScale,
    float topControlsDelta)
{
}

void WebLayerTreeViewImplForTesting::RequestNewOutputSurface()
{
    // Intentionally do not create and set an OutputSurface.
}

void WebLayerTreeViewImplForTesting::DidFailToInitializeOutputSurface()
{
    ASSERT_NOT_REACHED();
}

void WebLayerTreeViewImplForTesting::registerViewportLayers(
    const blink::WebLayer* overscrollElasticityLayer,
    const blink::WebLayer* pageScaleLayer,
    const blink::WebLayer* innerViewportScrollLayer,
    const blink::WebLayer* outerViewportScrollLayer)
{
    m_layerTreeHost->RegisterViewportLayers(
        // The scroll elasticity layer will only exist when using pinch virtual
        // viewports.
        overscrollElasticityLayer
            ? static_cast<const cc_blink::WebLayerImpl*>(overscrollElasticityLayer)->layer()
            : nullptr,
        static_cast<const cc_blink::WebLayerImpl*>(pageScaleLayer)->layer(),
        static_cast<const cc_blink::WebLayerImpl*>(innerViewportScrollLayer)->layer(),
        // The outer viewport layer will only exist when using pinch virtual
        // viewports.
        outerViewportScrollLayer
            ? static_cast<const cc_blink::WebLayerImpl*>(outerViewportScrollLayer)->layer()
            : nullptr);
}

void WebLayerTreeViewImplForTesting::clearViewportLayers()
{
    m_layerTreeHost->RegisterViewportLayers(scoped_refptr<cc::Layer>(),
        scoped_refptr<cc::Layer>(),
        scoped_refptr<cc::Layer>(),
        scoped_refptr<cc::Layer>());
}

void WebLayerTreeViewImplForTesting::registerSelection(const blink::WebSelection& selection)
{
}

void WebLayerTreeViewImplForTesting::clearSelection()
{
}

void WebLayerTreeViewImplForTesting::setEventListenerProperties(
    blink::WebEventListenerClass eventClass,
    blink::WebEventListenerProperties properties)
{
    // Equality of static_cast is checked in render_widget_compositor.cc.
    m_layerTreeHost->SetEventListenerProperties(
        static_cast<cc::EventListenerClass>(eventClass),
        static_cast<cc::EventListenerProperties>(properties));
}

blink::WebEventListenerProperties
WebLayerTreeViewImplForTesting::eventListenerProperties(blink::WebEventListenerClass eventClass) const
{
    // Equality of static_cast is checked in render_widget_compositor.cc.
    return static_cast<blink::WebEventListenerProperties>(
        m_layerTreeHost->event_listener_properties(
            static_cast<cc::EventListenerClass>(eventClass)));
}

void WebLayerTreeViewImplForTesting::setHaveScrollEventHandlers(bool haveEentHandlers)
{
    m_layerTreeHost->SetHaveScrollEventHandlers(haveEentHandlers);
}

bool WebLayerTreeViewImplForTesting::haveScrollEventHandlers() const
{
    return m_layerTreeHost->have_scroll_event_handlers();
}

} // namespace blink
