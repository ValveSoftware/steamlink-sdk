/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "web/LinkHighlightImpl.h"

#include "core/dom/DOMNodeIds.h"
#include "core/dom/LayoutTreeBuilderTraversal.h"
#include "core/dom/Node.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/paint/PaintLayer.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/animation/CompositorAnimation.h"
#include "platform/animation/CompositorAnimationCurve.h"
#include "platform/animation/CompositorFloatAnimationCurve.h"
#include "platform/animation/CompositorTargetProperty.h"
#include "platform/animation/TimingFunction.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/CompositorElementId.h"
#include "platform/graphics/CompositorMutableProperties.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebContentLayer.h"
#include "public/platform/WebDisplayItemList.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebSize.h"
#include "public/web/WebKit.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/gfx/geometry/rect.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebSettingsImpl.h"
#include "web/WebViewImpl.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

std::unique_ptr<LinkHighlightImpl> LinkHighlightImpl::create(Node* node, WebViewImpl* owningWebViewImpl)
{
    return wrapUnique(new LinkHighlightImpl(node, owningWebViewImpl));
}

LinkHighlightImpl::LinkHighlightImpl(Node* node, WebViewImpl* owningWebViewImpl)
    : m_node(node)
    , m_owningWebViewImpl(owningWebViewImpl)
    , m_currentGraphicsLayer(0)
    , m_isScrollingGraphicsLayer(false)
    , m_geometryNeedsUpdate(false)
    , m_isAnimating(false)
    , m_startTime(monotonicallyIncreasingTime())
{
    DCHECK(m_node);
    DCHECK(owningWebViewImpl);
    WebCompositorSupport* compositorSupport = Platform::current()->compositorSupport();
    DCHECK(compositorSupport);
    m_contentLayer = wrapUnique(compositorSupport->createContentLayer(this));
    m_clipLayer = wrapUnique(compositorSupport->createLayer());
    m_clipLayer->setTransformOrigin(WebFloatPoint3D());
    m_clipLayer->addChild(m_contentLayer->layer());

    m_compositorPlayer = CompositorAnimationPlayer::create();
    DCHECK(m_compositorPlayer);
    m_compositorPlayer->setAnimationDelegate(this);
    if (m_owningWebViewImpl->linkHighlightsTimeline())
        m_owningWebViewImpl->linkHighlightsTimeline()->playerAttached(*this);

    CompositorElementId elementId = createCompositorElementId(DOMNodeIds::idForNode(node), CompositorSubElementId::LinkHighlight);
    m_compositorPlayer->attachElement(elementId);
    m_contentLayer->layer()->setDrawsContent(true);
    m_contentLayer->layer()->setOpacity(1);
    m_contentLayer->layer()->setElementId(elementId);
    m_geometryNeedsUpdate = true;
}

LinkHighlightImpl::~LinkHighlightImpl()
{
    if (m_compositorPlayer->isElementAttached())
        m_compositorPlayer->detachElement();
    if (m_owningWebViewImpl->linkHighlightsTimeline())
        m_owningWebViewImpl->linkHighlightsTimeline()->playerDestroyed(*this);
    m_compositorPlayer->setAnimationDelegate(nullptr);
    m_compositorPlayer.reset();

    clearGraphicsLayerLinkHighlightPointer();
    releaseResources();
}

WebContentLayer* LinkHighlightImpl::contentLayer()
{
    return m_contentLayer.get();
}

WebLayer* LinkHighlightImpl::clipLayer()
{
    return m_clipLayer.get();
}

void LinkHighlightImpl::releaseResources()
{
    m_node.clear();
}

void LinkHighlightImpl::attachLinkHighlightToCompositingLayer(const LayoutBoxModelObject& paintInvalidationContainer)
{
    GraphicsLayer* newGraphicsLayer = paintInvalidationContainer.layer()->graphicsLayerBacking();
    m_isScrollingGraphicsLayer = false;
    // FIXME: There should always be a GraphicsLayer. See crbug.com/431961.
    if (paintInvalidationContainer.layer()->needsCompositedScrolling() && m_node->layoutObject() != &paintInvalidationContainer) {
        newGraphicsLayer = paintInvalidationContainer.layer()->graphicsLayerBackingForScrolling();
        m_isScrollingGraphicsLayer = true;
    }
    if (!newGraphicsLayer)
        return;

    m_clipLayer->setTransform(SkMatrix44(SkMatrix44::kIdentity_Constructor));

    if (m_currentGraphicsLayer != newGraphicsLayer) {
        if (m_currentGraphicsLayer)
            clearGraphicsLayerLinkHighlightPointer();

        m_currentGraphicsLayer = newGraphicsLayer;
        m_currentGraphicsLayer->addLinkHighlight(this);
    }
}

static void convertTargetSpaceQuadToCompositedLayer(const FloatQuad& targetSpaceQuad, LayoutObject* targetLayoutObject, const LayoutBoxModelObject& paintInvalidationContainer, FloatQuad& compositedSpaceQuad)
{
    DCHECK(targetLayoutObject);
    for (unsigned i = 0; i < 4; ++i) {
        IntPoint point;
        switch (i) {
        case 0: point = roundedIntPoint(targetSpaceQuad.p1()); break;
        case 1: point = roundedIntPoint(targetSpaceQuad.p2()); break;
        case 2: point = roundedIntPoint(targetSpaceQuad.p3()); break;
        case 3: point = roundedIntPoint(targetSpaceQuad.p4()); break;
        }

        // FIXME: this does not need to be absolute, just in the paint invalidation container's space.
        point = targetLayoutObject->frame()->view()->contentsToRootFrame(point);
        point = paintInvalidationContainer.frame()->view()->rootFrameToContents(point);
        FloatPoint floatPoint = paintInvalidationContainer.absoluteToLocal(point, UseTransforms);
        PaintLayer::mapPointInPaintInvalidationContainerToBacking(paintInvalidationContainer, floatPoint);

        switch (i) {
        case 0: compositedSpaceQuad.setP1(floatPoint); break;
        case 1: compositedSpaceQuad.setP2(floatPoint); break;
        case 2: compositedSpaceQuad.setP3(floatPoint); break;
        case 3: compositedSpaceQuad.setP4(floatPoint); break;
        }
    }
}

static void addQuadToPath(const FloatQuad& quad, Path& path)
{
    // FIXME: Make this create rounded quad-paths, just like the axis-aligned case.
    path.moveTo(quad.p1());
    path.addLineTo(quad.p2());
    path.addLineTo(quad.p3());
    path.addLineTo(quad.p4());
    path.closeSubpath();
}

void LinkHighlightImpl::computeQuads(const Node& node, Vector<FloatQuad>& outQuads) const
{
    if (!node.layoutObject())
        return;

    LayoutObject* layoutObject = node.layoutObject();

    // For inline elements, absoluteQuads will return a line box based on the line-height
    // and font metrics, which is technically incorrect as replaced elements like images
    // should use their intristic height and expand the linebox  as needed. To get an
    // appropriately sized highlight we descend into the children and have them add their
    // boxes.
    if (layoutObject->isLayoutInline()) {
        for (Node* child = LayoutTreeBuilderTraversal::firstChild(node); child; child = LayoutTreeBuilderTraversal::nextSibling(*child))
            computeQuads(*child, outQuads);
    } else {
        // FIXME: this does not need to be absolute, just in the paint invalidation container's space.
        layoutObject->absoluteQuads(outQuads);
    }
}

bool LinkHighlightImpl::computeHighlightLayerPathAndPosition(const LayoutBoxModelObject& paintInvalidationContainer)
{
    if (!m_node || !m_node->layoutObject() || !m_currentGraphicsLayer)
        return false;

    // FIXME: This is defensive code to avoid crashes such as those described in
    // crbug.com/440887. This should be cleaned up once we fix the root cause of
    // of the paint invalidation container not being composited.
    if (!paintInvalidationContainer.layer()->compositedLayerMapping() && !paintInvalidationContainer.layer()->groupedMapping())
        return false;

    // Get quads for node in absolute coordinates.
    Vector<FloatQuad> quads;
    computeQuads(*m_node, quads);
    DCHECK(quads.size());
    Path newPath;

    for (size_t quadIndex = 0; quadIndex < quads.size(); ++quadIndex) {
        FloatQuad absoluteQuad = quads[quadIndex];

        // Scrolling content layers have the same offset from layout object as the non-scrolling layers. Thus we need
        // to adjust for their scroll offset.
        if (m_isScrollingGraphicsLayer) {
            DoubleSize adjustedScrollOffset = paintInvalidationContainer.layer()->getScrollableArea()->adjustedScrollOffset();
            absoluteQuad.move(adjustedScrollOffset.width(), adjustedScrollOffset.height());
        }

        // Transform node quads in target absolute coords to local coordinates in the compositor layer.
        FloatQuad transformedQuad;
        convertTargetSpaceQuadToCompositedLayer(absoluteQuad, m_node->layoutObject(), paintInvalidationContainer, transformedQuad);

        // FIXME: for now, we'll only use rounded paths if we have a single node quad. The reason for this is that
        // we may sometimes get a chain of adjacent boxes (e.g. for text nodes) which end up looking like sausage
        // links: these should ideally be merged into a single rect before creating the path, but that's
        // another CL.
        if (quads.size() == 1 && transformedQuad.isRectilinear()
            && !m_owningWebViewImpl->settingsImpl()->mockGestureTapHighlightsEnabled()) {
            FloatSize rectRoundingRadii(3, 3);
            newPath.addRoundedRect(transformedQuad.boundingBox(), rectRoundingRadii);
        } else {
            addQuadToPath(transformedQuad, newPath);
        }
    }

    FloatRect boundingRect = newPath.boundingRect();
    newPath.translate(-toFloatSize(boundingRect.location()));

    bool pathHasChanged = !(newPath == m_path);
    if (pathHasChanged) {
        m_path = newPath;
        m_contentLayer->layer()->setBounds(enclosingIntRect(boundingRect).size());
    }

    m_contentLayer->layer()->setPosition(boundingRect.location());

    return pathHasChanged;
}

gfx::Rect LinkHighlightImpl::paintableRegion()
{
    return gfx::Rect(0, 0, contentLayer()->layer()->bounds().width, contentLayer()->layer()->bounds().height);
}

void LinkHighlightImpl::paintContents(WebDisplayItemList* webDisplayItemList, WebContentLayerClient::PaintingControlSetting paintingControl)
{
    if (!m_node || !m_node->layoutObject())
        return;

    SkPictureRecorder recorder;
    gfx::Rect visualRect = paintableRegion();
    SkCanvas* canvas = recorder.beginRecording(visualRect.width(), visualRect.height());

    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setFlags(SkPaint::kAntiAlias_Flag);
    paint.setColor(m_node->layoutObject()->style()->tapHighlightColor().rgb());
    canvas->drawPath(m_path.getSkPath(), paint);

    webDisplayItemList->appendDrawingItem(WebRect(visualRect.x(), visualRect.y(),
        visualRect.width(), visualRect.height()), recorder.finishRecordingAsPicture());
}

void LinkHighlightImpl::startHighlightAnimationIfNeeded()
{
    if (m_isAnimating)
        return;

    m_isAnimating = true;
    const float startOpacity = 1;
    // FIXME: Should duration be configurable?
    const float fadeDuration = 0.1f;
    const float minPreFadeDuration = 0.1f;

    m_contentLayer->layer()->setOpacity(startOpacity);

    std::unique_ptr<CompositorFloatAnimationCurve> curve = CompositorFloatAnimationCurve::create();

    const auto easeType = CubicBezierTimingFunction::EaseType::EASE;

    curve->addCubicBezierKeyframe(CompositorFloatKeyframe(0, startOpacity), easeType);
    // Make sure we have displayed for at least minPreFadeDuration before starting to fade out.
    float extraDurationRequired = std::max(0.f, minPreFadeDuration - static_cast<float>(monotonicallyIncreasingTime() - m_startTime));
    if (extraDurationRequired)
        curve->addCubicBezierKeyframe(CompositorFloatKeyframe(extraDurationRequired, startOpacity), easeType);
    // For layout tests we don't fade out.
    curve->addCubicBezierKeyframe(CompositorFloatKeyframe(fadeDuration + extraDurationRequired, layoutTestMode() ? startOpacity : 0), easeType);

    std::unique_ptr<CompositorAnimation> animation = CompositorAnimation::create(*curve, CompositorTargetProperty::OPACITY, 0, 0);

    m_contentLayer->layer()->setDrawsContent(true);
    m_compositorPlayer->addAnimation(animation.release());

    invalidate();
    m_owningWebViewImpl->scheduleAnimation();
}

void LinkHighlightImpl::clearGraphicsLayerLinkHighlightPointer()
{
    if (m_currentGraphicsLayer) {
        m_currentGraphicsLayer->removeLinkHighlight(this);
        m_currentGraphicsLayer = 0;
    }
}

void LinkHighlightImpl::notifyAnimationStarted(double, int)
{
}

void LinkHighlightImpl::notifyAnimationFinished(double, int)
{
    // Since WebViewImpl may hang on to us for a while, make sure we
    // release resources as soon as possible.
    clearGraphicsLayerLinkHighlightPointer();
    releaseResources();
}

class LinkHighlightDisplayItemClientForTracking : public DisplayItemClient {
    String debugName() const final { return "LinkHighlight"; }
    LayoutRect visualRect() const final { return LayoutRect(); }
};

void LinkHighlightImpl::updateGeometry()
{
    // To avoid unnecessary updates (e.g. other entities have requested animations from our WebViewImpl),
    // only proceed if we actually requested an update.
    if (!m_geometryNeedsUpdate)
        return;

    m_geometryNeedsUpdate = false;

    bool hasLayoutObject = m_node && m_node->layoutObject();
    if (hasLayoutObject) {
        const LayoutBoxModelObject& paintInvalidationContainer = m_node->layoutObject()->containerForPaintInvalidation();
        attachLinkHighlightToCompositingLayer(paintInvalidationContainer);
        if (computeHighlightLayerPathAndPosition(paintInvalidationContainer)) {
            // We only need to invalidate the layer if the highlight size has changed, otherwise
            // we can just re-position the layer without needing to repaint.
            m_contentLayer->layer()->invalidate();

            if (m_currentGraphicsLayer)
                m_currentGraphicsLayer->trackPaintInvalidation(LinkHighlightDisplayItemClientForTracking(), enclosingIntRect(FloatRect(layer()->position().x, layer()->position().y, layer()->bounds().width, layer()->bounds().height)), PaintInvalidationFull);
        }
    } else {
        clearGraphicsLayerLinkHighlightPointer();
        releaseResources();
    }
}

void LinkHighlightImpl::clearCurrentGraphicsLayer()
{
    m_currentGraphicsLayer = 0;
    m_geometryNeedsUpdate = true;
}

void LinkHighlightImpl::invalidate()
{
    // Make sure we update geometry on the next callback from WebViewImpl::layout().
    m_geometryNeedsUpdate = true;
}

WebLayer* LinkHighlightImpl::layer()
{
    return clipLayer();
}

CompositorAnimationPlayer* LinkHighlightImpl::compositorPlayer() const
{
    return m_compositorPlayer.get();
}

} // namespace blink
