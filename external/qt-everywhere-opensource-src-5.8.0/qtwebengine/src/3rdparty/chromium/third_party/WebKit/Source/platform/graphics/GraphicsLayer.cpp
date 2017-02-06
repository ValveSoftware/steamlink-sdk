/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/GraphicsLayer.h"

#include "SkImageFilter.h"
#include "SkMatrix44.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/layers/layer.h"
#include "platform/DragImage.h"
#include "platform/JSONValues.h"
#include "platform/TraceEvent.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/geometry/Region.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/CompositorFilterOperations.h"
#include "platform/graphics/FirstPaintInvalidationTracking.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/LinkHighlight.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/scroll/ScrollableArea.h"
#include "platform/text/TextStream.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebSize.h"
#include "wtf/CurrentTime.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/MathExtras.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/StringUTF8Adaptor.h"
#include "wtf/text/WTFString.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace blink {

static bool s_drawDebugRedFill = true;

struct PaintInvalidationInfo {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    // This is for comparison only. Don't dereference because the client may have died.
    const DisplayItemClient* client;
    String clientDebugName;
    IntRect rect;
    PaintInvalidationReason reason;
};

#if DCHECK_IS_ON()
struct UnderPaintInvalidation {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    int x;
    int y;
    SkColor oldPixel;
    SkColor newPixel;
};
#endif

struct PaintInvalidationTracking {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    Vector<PaintInvalidationInfo> trackedPaintInvalidations;
#if DCHECK_IS_ON()
    RefPtr<SkPicture> lastPaintedPicture;
    Region paintInvalidationRegionSinceLastPaint;
    Vector<UnderPaintInvalidation> underPaintInvalidations;
#endif
};

typedef HashMap<const GraphicsLayer*, PaintInvalidationTracking> PaintInvalidationTrackingMap;
static PaintInvalidationTrackingMap& paintInvalidationTrackingMap()
{
    DEFINE_STATIC_LOCAL(PaintInvalidationTrackingMap, map, ());
    return map;
}

std::unique_ptr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerClient* client)
{
    return wrapUnique(new GraphicsLayer(client));
}

GraphicsLayer::GraphicsLayer(GraphicsLayerClient* client)
    : m_client(client)
    , m_backgroundColor(Color::transparent)
    , m_opacity(1)
    , m_blendMode(WebBlendModeNormal)
    , m_hasTransformOrigin(false)
    , m_contentsOpaque(false)
    , m_shouldFlattenTransform(true)
    , m_backfaceVisibility(true)
    , m_masksToBounds(false)
    , m_drawsContent(false)
    , m_contentsVisible(true)
    , m_isRootForIsolatedGroup(false)
    , m_hasScrollParent(false)
    , m_hasClipParent(false)
    , m_painted(false)
    , m_textPainted(false)
    , m_imagePainted(false)
    , m_isTrackingPaintInvalidations(client && client->isTrackingPaintInvalidations())
    , m_paintingPhase(GraphicsLayerPaintAllWithOverflowClip)
    , m_parent(0)
    , m_maskLayer(0)
    , m_contentsClippingMaskLayer(0)
    , m_replicaLayer(0)
    , m_replicatedLayer(0)
    , m_paintCount(0)
    , m_contentsLayer(0)
    , m_contentsLayerId(0)
    , m_scrollableArea(nullptr)
    , m_3dRenderingContext(0)
{
#if ENABLE(ASSERT)
    if (m_client)
        m_client->verifyNotPainting();
#endif

    m_contentLayerDelegate = wrapUnique(new ContentLayerDelegate(this));
    m_layer = wrapUnique(Platform::current()->compositorSupport()->createContentLayer(m_contentLayerDelegate.get()));
    m_layer->layer()->setDrawsContent(m_drawsContent && m_contentsVisible);
    m_layer->layer()->setLayerClient(this);
}

GraphicsLayer::~GraphicsLayer()
{
    for (size_t i = 0; i < m_linkHighlights.size(); ++i)
        m_linkHighlights[i]->clearCurrentGraphicsLayer();
    m_linkHighlights.clear();

#if ENABLE(ASSERT)
    if (m_client)
        m_client->verifyNotPainting();
#endif

    if (m_replicaLayer)
        m_replicaLayer->setReplicatedLayer(0);

    if (m_replicatedLayer)
        m_replicatedLayer->setReplicatedByLayer(0);

    removeAllChildren();
    removeFromParent();

    paintInvalidationTrackingMap().remove(this);
    ASSERT(!m_parent);
}

LayoutRect GraphicsLayer::visualRect() const
{
    LayoutRect bounds = LayoutRect(FloatPoint(), size());
    bounds.move(offsetFromLayoutObjectWithSubpixelAccumulation());
    return bounds;
}

void GraphicsLayer::setHasWillChangeTransformHint(bool hasWillChangeTransform)
{
    m_layer->layer()->setHasWillChangeTransformHint(hasWillChangeTransform);
}

void GraphicsLayer::setDrawDebugRedFillForTesting(bool enabled)
{
    s_drawDebugRedFill = enabled;
}

void GraphicsLayer::setParent(GraphicsLayer* layer)
{
    ASSERT(!layer || !layer->hasAncestor(this));
    m_parent = layer;
}

#if ENABLE(ASSERT)

bool GraphicsLayer::hasAncestor(GraphicsLayer* ancestor) const
{
    for (GraphicsLayer* curr = parent(); curr; curr = curr->parent()) {
        if (curr == ancestor)
            return true;
    }

    return false;
}

#endif

bool GraphicsLayer::setChildren(const GraphicsLayerVector& newChildren)
{
    // If the contents of the arrays are the same, nothing to do.
    if (newChildren == m_children)
        return false;

    removeAllChildren();

    size_t listSize = newChildren.size();
    for (size_t i = 0; i < listSize; ++i)
        addChildInternal(newChildren[i]);

    updateChildList();

    return true;
}

void GraphicsLayer::addChildInternal(GraphicsLayer* childLayer)
{
    ASSERT(childLayer != this);

    if (childLayer->parent())
        childLayer->removeFromParent();

    childLayer->setParent(this);
    m_children.append(childLayer);

    // Don't call updateChildList here, this function is used in cases where it
    // should not be called until all children are processed.
}

void GraphicsLayer::addChild(GraphicsLayer* childLayer)
{
    addChildInternal(childLayer);
    updateChildList();
}

void GraphicsLayer::addChildBelow(GraphicsLayer* childLayer, GraphicsLayer* sibling)
{
    ASSERT(childLayer != this);
    childLayer->removeFromParent();

    bool found = false;
    for (unsigned i = 0; i < m_children.size(); i++) {
        if (sibling == m_children[i]) {
            m_children.insert(i, childLayer);
            found = true;
            break;
        }
    }

    childLayer->setParent(this);

    if (!found)
        m_children.append(childLayer);

    updateChildList();
}

void GraphicsLayer::removeAllChildren()
{
    while (!m_children.isEmpty()) {
        GraphicsLayer* curLayer = m_children.last();
        ASSERT(curLayer->parent());
        curLayer->removeFromParent();
    }
}

void GraphicsLayer::removeFromParent()
{
    if (m_parent) {
        // We use reverseFind so that removeAllChildren() isn't n^2.
        m_parent->m_children.remove(m_parent->m_children.reverseFind(this));
        setParent(0);
    }

    platformLayer()->removeFromParent();
}

void GraphicsLayer::setReplicatedByLayer(GraphicsLayer* layer)
{
    // FIXME: this could probably be a full early exit.
    if (m_replicaLayer != layer) {
        if (m_replicaLayer)
            m_replicaLayer->setReplicatedLayer(0);

        if (layer)
            layer->setReplicatedLayer(this);

        m_replicaLayer = layer;
    }

    WebLayer* webReplicaLayer = layer ? layer->platformLayer() : 0;
    platformLayer()->setReplicaLayer(webReplicaLayer);
}

void GraphicsLayer::setOffsetFromLayoutObject(const IntSize& offset, ShouldSetNeedsDisplay shouldSetNeedsDisplay)
{
    setOffsetDoubleFromLayoutObject(offset);
}

void GraphicsLayer::setOffsetDoubleFromLayoutObject(const DoubleSize& offset, ShouldSetNeedsDisplay shouldSetNeedsDisplay)
{
    if (offset == m_offsetFromLayoutObject)
        return;

    m_offsetFromLayoutObject = offset;

    // If the compositing layer offset changes, we need to repaint.
    if (shouldSetNeedsDisplay == SetNeedsDisplay)
        setNeedsDisplay();
}

LayoutSize GraphicsLayer::offsetFromLayoutObjectWithSubpixelAccumulation() const
{
    return LayoutSize(offsetFromLayoutObject()) + client()->subpixelAccumulation();
}

IntRect GraphicsLayer::interestRect()
{
    return m_previousInterestRect;
}

void GraphicsLayer::paint(const IntRect* interestRect, GraphicsContext::DisabledMode disabledMode)
{
    if (paintWithoutCommit(interestRect, disabledMode)) {
        getPaintController().commitNewDisplayItems(offsetFromLayoutObjectWithSubpixelAccumulation());
#if DCHECK_IS_ON()
        if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled()) {
            RefPtr<SkPicture> newPicture = capturePicture();
            checkPaintUnderInvalidations(*newPicture);
            PaintInvalidationTracking& tracking = paintInvalidationTrackingMap().add(this, PaintInvalidationTracking()).storedValue->value;
            tracking.lastPaintedPicture = newPicture;
            tracking.paintInvalidationRegionSinceLastPaint = Region();
        }
#endif
    }
}

bool GraphicsLayer::paintWithoutCommit(const IntRect* interestRect, GraphicsContext::DisabledMode disabledMode)
{
    ASSERT(drawsContent());

    if (!m_client)
        return false;
    if (firstPaintInvalidationTrackingEnabled())
        m_debugInfo.clearAnnotatedInvalidateRects();
    incrementPaintCount();

    IntRect newInterestRect;
    if (!interestRect) {
        newInterestRect = m_client->computeInterestRect(this, m_previousInterestRect);
        interestRect = &newInterestRect;
    }

    if (!getPaintController().subsequenceCachingIsDisabled()
        && !m_client->needsRepaint(*this)
        && !getPaintController().cacheIsEmpty()
        && m_previousInterestRect == *interestRect) {
        return false;
    }

    GraphicsContext context(getPaintController(), disabledMode);

#ifndef NDEBUG
    if (contentsOpaque() && s_drawDebugRedFill) {
        FloatRect rect(FloatPoint(), size());
        if (!DrawingRecorder::useCachedDrawingIfPossible(context, *this, DisplayItem::DebugRedFill)) {
            DrawingRecorder recorder(context, *this, DisplayItem::DebugRedFill, rect);
            context.fillRect(rect, SK_ColorRED);
        }
    }
#endif

    m_previousInterestRect = *interestRect;
    m_client->paintContents(this, context, m_paintingPhase, *interestRect);
    notifyFirstPaintToClient();
    return true;
}

void GraphicsLayer::notifyFirstPaintToClient()
{
    if (!m_painted) {
        DisplayItemList& itemList = m_paintController->newDisplayItemList();
        for (DisplayItem& item : itemList) {
            DisplayItem::Type type = item.getType();
            if (DisplayItem::isDrawingType(type) && type != DisplayItem::DocumentBackground && type != DisplayItem::DebugRedFill && static_cast<const DrawingDisplayItem&>(item).picture()) {
                m_painted = true;
                m_client->notifyFirstPaint();
                break;
            }
        }
    }
    if (!m_textPainted && m_paintController->textPainted()) {
        m_textPainted = true;
        m_client->notifyFirstTextPaint();
    }
    if (!m_imagePainted && m_paintController->imagePainted()) {
        m_imagePainted = true;
        m_client->notifyFirstImagePaint();
    }
}

void GraphicsLayer::updateChildList()
{
    WebLayer* childHost = m_layer->layer();
    childHost->removeAllChildren();

    clearContentsLayerIfUnregistered();

    if (m_contentsLayer) {
        // FIXME: add the contents layer in the correct order with negative z-order children.
        // This does not cause visible rendering issues because currently contents layers are only used
        // for replaced elements that don't have children.
        childHost->addChild(m_contentsLayer);
    }

    for (size_t i = 0; i < m_children.size(); ++i)
        childHost->addChild(m_children[i]->platformLayer());

    for (size_t i = 0; i < m_linkHighlights.size(); ++i)
        childHost->addChild(m_linkHighlights[i]->layer());
}

void GraphicsLayer::updateLayerIsDrawable()
{
    // For the rest of the accelerated compositor code, there is no reason to make a
    // distinction between drawsContent and contentsVisible. So, for m_layer->layer(), these two
    // flags are combined here. m_contentsLayer shouldn't receive the drawsContent flag
    // so it is only given contentsVisible.

    m_layer->layer()->setDrawsContent(m_drawsContent && m_contentsVisible);
    if (WebLayer* contentsLayer = contentsLayerIfRegistered())
        contentsLayer->setDrawsContent(m_contentsVisible);

    if (m_drawsContent) {
        m_layer->layer()->invalidate();
        for (size_t i = 0; i < m_linkHighlights.size(); ++i)
            m_linkHighlights[i]->invalidate();
    }
}

void GraphicsLayer::updateContentsRect()
{
    WebLayer* contentsLayer = contentsLayerIfRegistered();
    if (!contentsLayer)
        return;

    contentsLayer->setPosition(FloatPoint(m_contentsRect.x(), m_contentsRect.y()));
    contentsLayer->setBounds(IntSize(m_contentsRect.width(), m_contentsRect.height()));

    if (m_contentsClippingMaskLayer) {
        if (m_contentsClippingMaskLayer->size() != m_contentsRect.size()) {
            m_contentsClippingMaskLayer->setSize(FloatSize(m_contentsRect.size()));
            m_contentsClippingMaskLayer->setNeedsDisplay();
        }
        m_contentsClippingMaskLayer->setPosition(FloatPoint());
        m_contentsClippingMaskLayer->setOffsetFromLayoutObject(offsetFromLayoutObject() + IntSize(m_contentsRect.location().x(), m_contentsRect.location().y()));
    }
}

static HashSet<int>* s_registeredLayerSet;

void GraphicsLayer::registerContentsLayer(WebLayer* layer)
{
    if (!s_registeredLayerSet)
        s_registeredLayerSet = new HashSet<int>;
    if (s_registeredLayerSet->contains(layer->id()))
        CRASH();
    s_registeredLayerSet->add(layer->id());
}

void GraphicsLayer::unregisterContentsLayer(WebLayer* layer)
{
    ASSERT(s_registeredLayerSet);
    if (!s_registeredLayerSet->contains(layer->id()))
        CRASH();
    s_registeredLayerSet->remove(layer->id());
}

void GraphicsLayer::setContentsTo(WebLayer* layer)
{
    bool childrenChanged = false;
    if (layer) {
        ASSERT(s_registeredLayerSet);
        if (!s_registeredLayerSet->contains(layer->id()))
            CRASH();
        if (m_contentsLayerId != layer->id()) {
            setupContentsLayer(layer);
            childrenChanged = true;
        }
        updateContentsRect();
    } else {
        if (m_contentsLayer) {
            childrenChanged = true;

            // The old contents layer will be removed via updateChildList.
            m_contentsLayer = 0;
            m_contentsLayerId = 0;
        }
    }

    if (childrenChanged)
        updateChildList();
}

void GraphicsLayer::setupContentsLayer(WebLayer* contentsLayer)
{
    ASSERT(contentsLayer);
    m_contentsLayer = contentsLayer;
    m_contentsLayerId = m_contentsLayer->id();

    m_contentsLayer->setLayerClient(this);
    m_contentsLayer->setTransformOrigin(FloatPoint3D());
    m_contentsLayer->setUseParentBackfaceVisibility(true);

    // It is necessary to call setDrawsContent as soon as we receive the new contentsLayer, for
    // the correctness of early exit conditions in setDrawsContent() and setContentsVisible().
    m_contentsLayer->setDrawsContent(m_contentsVisible);

    // Insert the content layer first. Video elements require this, because they have
    // shadow content that must display in front of the video.
    m_layer->layer()->insertChild(m_contentsLayer, 0);
    WebLayer* borderWebLayer = m_contentsClippingMaskLayer ? m_contentsClippingMaskLayer->platformLayer() : 0;
    m_contentsLayer->setMaskLayer(borderWebLayer);

    m_contentsLayer->setRenderingContext(m_3dRenderingContext);
}

void GraphicsLayer::clearContentsLayerIfUnregistered()
{
    if (!m_contentsLayerId || s_registeredLayerSet->contains(m_contentsLayerId))
        return;

    m_contentsLayer = 0;
    m_contentsLayerId = 0;
}

GraphicsLayerDebugInfo& GraphicsLayer::debugInfo()
{
    return m_debugInfo;
}

WebLayer* GraphicsLayer::contentsLayerIfRegistered()
{
    clearContentsLayerIfUnregistered();
    return m_contentsLayer;
}

void GraphicsLayer::setTracksPaintInvalidations(bool tracksPaintInvalidations)
{
    resetTrackedPaintInvalidations();
    m_isTrackingPaintInvalidations = tracksPaintInvalidations;
}

void GraphicsLayer::resetTrackedPaintInvalidations()
{
    auto it = paintInvalidationTrackingMap().find(this);
    if (it == paintInvalidationTrackingMap().end())
        return;

    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled())
        it->value.trackedPaintInvalidations.clear();
    else
        paintInvalidationTrackingMap().remove(it);
}

bool GraphicsLayer::hasTrackedPaintInvalidations() const
{
    PaintInvalidationTrackingMap::iterator it = paintInvalidationTrackingMap().find(this);
    if (it != paintInvalidationTrackingMap().end())
        return !it->value.trackedPaintInvalidations.isEmpty();
    return false;
}

void GraphicsLayer::trackPaintInvalidation(const DisplayItemClient& client, const IntRect& rect, PaintInvalidationReason reason)
{
    if (!isTrackingOrCheckingPaintInvalidations() || rect.isEmpty())
        return;

    PaintInvalidationTracking& tracking = paintInvalidationTrackingMap().add(this, PaintInvalidationTracking()).storedValue->value;

    if (m_isTrackingPaintInvalidations) {
        PaintInvalidationInfo info = { &client, client.debugName(), rect, reason };
        tracking.trackedPaintInvalidations.append(info);
    }

#if DCHECK_IS_ON()
    if (RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled()) {
        // TODO(crbug.com/496260): Some antialiasing effects overflows the paint invalidation rect.
        IntRect r = rect;
        r.inflate(1);
        tracking.paintInvalidationRegionSinceLastPaint.unite(r);
    }
#endif
}

static bool comparePaintInvalidationInfo(const PaintInvalidationInfo& a, const PaintInvalidationInfo& b)
{
    // Sort by rect first, bigger rects before smaller ones.
    if (a.rect.width() != b.rect.width())
        return a.rect.width() > b.rect.width();
    if (a.rect.height() != b.rect.height())
        return a.rect.height() > b.rect.height();
    if (a.rect.x() != b.rect.x())
        return a.rect.x() > b.rect.x();
    if (a.rect.y() != b.rect.y())
        return a.rect.y() > b.rect.y();

    // Then compare clientDebugName, in alphabetic order.
    int nameCompareResult = codePointCompare(a.clientDebugName, b.clientDebugName);
    if (nameCompareResult != 0)
        return nameCompareResult < 0;

    return a.reason < b.reason;
}

template <typename T>
static PassRefPtr<JSONArray> pointAsJSONArray(const T& point)
{
    RefPtr<JSONArray> array = JSONArray::create();
    array->pushNumber(point.x());
    array->pushNumber(point.y());
    return array;
}

template <typename T>
static PassRefPtr<JSONArray> sizeAsJSONArray(const T& size)
{
    RefPtr<JSONArray> array = JSONArray::create();
    array->pushNumber(size.width());
    array->pushNumber(size.height());
    return array;
}

template <typename T>
static PassRefPtr<JSONArray> rectAsJSONArray(const T& rect)
{
    RefPtr<JSONArray> array = JSONArray::create();
    array->pushNumber(rect.x());
    array->pushNumber(rect.y());
    array->pushNumber(rect.width());
    array->pushNumber(rect.height());
    return array;
}

static double roundCloseToZero(double number)
{
    return std::abs(number) < 1e-7 ? 0 : number;
}

static PassRefPtr<JSONArray> transformAsJSONArray(const TransformationMatrix& t)
{
    RefPtr<JSONArray> array = JSONArray::create();
    {
        RefPtr<JSONArray> row = JSONArray::create();
        row->pushNumber(roundCloseToZero(t.m11()));
        row->pushNumber(roundCloseToZero(t.m12()));
        row->pushNumber(roundCloseToZero(t.m13()));
        row->pushNumber(roundCloseToZero(t.m14()));
        array->pushArray(row);
    }
    {
        RefPtr<JSONArray> row = JSONArray::create();
        row->pushNumber(roundCloseToZero(t.m21()));
        row->pushNumber(roundCloseToZero(t.m22()));
        row->pushNumber(roundCloseToZero(t.m23()));
        row->pushNumber(roundCloseToZero(t.m24()));
        array->pushArray(row);
    }
    {
        RefPtr<JSONArray> row = JSONArray::create();
        row->pushNumber(roundCloseToZero(t.m31()));
        row->pushNumber(roundCloseToZero(t.m32()));
        row->pushNumber(roundCloseToZero(t.m33()));
        row->pushNumber(roundCloseToZero(t.m34()));
        array->pushArray(row);
    }
    {
        RefPtr<JSONArray> row = JSONArray::create();
        row->pushNumber(roundCloseToZero(t.m41()));
        row->pushNumber(roundCloseToZero(t.m42()));
        row->pushNumber(roundCloseToZero(t.m43()));
        row->pushNumber(roundCloseToZero(t.m44()));
        array->pushArray(row);
    }
    return array;
}

static String pointerAsString(const void* ptr)
{
    TextStream ts;
    ts << ptr;
    return ts.release();
}

PassRefPtr<JSONObject> GraphicsLayer::layerTreeAsJSON(LayerTreeFlags flags) const
{
    RenderingContextMap renderingContextMap;
    return layerTreeAsJSONInternal(flags, renderingContextMap);
}

PassRefPtr<JSONObject> GraphicsLayer::layerTreeAsJSONInternal(LayerTreeFlags flags, RenderingContextMap& renderingContextMap) const
{
    RefPtr<JSONObject> json = JSONObject::create();

    if (flags & LayerTreeIncludesDebugInfo)
        json->setString("this", pointerAsString(this));

    json->setString("name", debugName());

    if (m_position != FloatPoint())
        json->setArray("position", pointAsJSONArray(m_position));

    if (m_hasTransformOrigin && m_transformOrigin != FloatPoint3D(m_size.width() * 0.5f, m_size.height() * 0.5f, 0))
        json->setArray("transformOrigin", pointAsJSONArray(m_transformOrigin));

    if (m_size != IntSize())
        json->setArray("bounds", sizeAsJSONArray(m_size));

    if (m_opacity != 1)
        json->setNumber("opacity", m_opacity);

    if (m_blendMode != WebBlendModeNormal)
        json->setString("blendMode", compositeOperatorName(CompositeSourceOver, m_blendMode));

    if (m_isRootForIsolatedGroup)
        json->setBoolean("isolate", m_isRootForIsolatedGroup);

    if (m_contentsOpaque)
        json->setBoolean("contentsOpaque", m_contentsOpaque);

    if (!m_shouldFlattenTransform)
        json->setBoolean("shouldFlattenTransform", m_shouldFlattenTransform);

    if (m_3dRenderingContext) {
        RenderingContextMap::const_iterator it = renderingContextMap.find(m_3dRenderingContext);
        int contextId = renderingContextMap.size() + 1;
        if (it == renderingContextMap.end())
            renderingContextMap.set(m_3dRenderingContext, contextId);
        else
            contextId = it->value;

        json->setNumber("3dRenderingContext", contextId);
    }

    if (m_drawsContent)
        json->setBoolean("drawsContent", m_drawsContent);

    if (!m_contentsVisible)
        json->setBoolean("contentsVisible", m_contentsVisible);

    if (!m_backfaceVisibility)
        json->setString("backfaceVisibility", m_backfaceVisibility ? "visible" : "hidden");

    if (flags & LayerTreeIncludesDebugInfo)
        json->setString("client", pointerAsString(m_client));

    if (m_backgroundColor.alpha())
        json->setString("backgroundColor", m_backgroundColor.nameForLayoutTreeAsText());

    if (!m_transform.isIdentity())
        json->setArray("transform", transformAsJSONArray(m_transform));

    if (m_replicaLayer)
        json->setObject("replicaLayer", m_replicaLayer->layerTreeAsJSONInternal(flags, renderingContextMap));

    if (m_replicatedLayer)
        json->setString("replicatedLayer", flags & LayerTreeIncludesDebugInfo ? pointerAsString(m_replicatedLayer) : "");

    PaintInvalidationTrackingMap::iterator it = paintInvalidationTrackingMap().find(this);
    if (it != paintInvalidationTrackingMap().end()) {
        if (flags & LayerTreeIncludesPaintInvalidations) {
            Vector<PaintInvalidationInfo>& infos = it->value.trackedPaintInvalidations;
            if (!infos.isEmpty()) {
                std::sort(infos.begin(), infos.end(), &comparePaintInvalidationInfo);
                RefPtr<JSONArray> paintInvalidationsJSON = JSONArray::create();
                for (auto& info : infos) {
                    RefPtr<JSONObject> infoJSON = JSONObject::create();
                    infoJSON->setString("object", info.clientDebugName);
                    if (!info.rect.isEmpty())
                        infoJSON->setArray("rect", rectAsJSONArray(info.rect));
                    infoJSON->setString("reason", paintInvalidationReasonToString(info.reason));
                    paintInvalidationsJSON->pushObject(infoJSON);
                }
                json->setArray("paintInvalidations", paintInvalidationsJSON);
            }
#if DCHECK_IS_ON()
            Vector<UnderPaintInvalidation>& underPaintInvalidations = it->value.underPaintInvalidations;
            if (!underPaintInvalidations.isEmpty()) {
                RefPtr<JSONArray> underPaintInvalidationsJSON = JSONArray::create();
                for (auto& underPaintInvalidation : underPaintInvalidations) {
                    RefPtr<JSONObject> underPaintInvalidationJSON = JSONObject::create();
                    underPaintInvalidationJSON->setNumber("x", underPaintInvalidation.x);
                    underPaintInvalidationJSON->setNumber("y", underPaintInvalidation.x);
                    underPaintInvalidationJSON->setString("oldPixel", Color(underPaintInvalidation.oldPixel).nameForLayoutTreeAsText());
                    underPaintInvalidationJSON->setString("newPixel", Color(underPaintInvalidation.newPixel).nameForLayoutTreeAsText());
                    underPaintInvalidationsJSON->pushObject(underPaintInvalidationJSON);
                }
                json->setArray("underPaintInvalidations", underPaintInvalidationsJSON);
            }
#endif
        }
    }

    if ((flags & LayerTreeIncludesPaintingPhases) && m_paintingPhase) {
        RefPtr<JSONArray> paintingPhasesJSON = JSONArray::create();
        if (m_paintingPhase & GraphicsLayerPaintBackground)
            paintingPhasesJSON->pushString("GraphicsLayerPaintBackground");
        if (m_paintingPhase & GraphicsLayerPaintForeground)
            paintingPhasesJSON->pushString("GraphicsLayerPaintForeground");
        if (m_paintingPhase & GraphicsLayerPaintMask)
            paintingPhasesJSON->pushString("GraphicsLayerPaintMask");
        if (m_paintingPhase & GraphicsLayerPaintChildClippingMask)
            paintingPhasesJSON->pushString("GraphicsLayerPaintChildClippingMask");
        if (m_paintingPhase & GraphicsLayerPaintOverflowContents)
            paintingPhasesJSON->pushString("GraphicsLayerPaintOverflowContents");
        if (m_paintingPhase & GraphicsLayerPaintCompositedScroll)
            paintingPhasesJSON->pushString("GraphicsLayerPaintCompositedScroll");
        json->setArray("paintingPhases", paintingPhasesJSON);
    }

    if (flags & LayerTreeIncludesClipAndScrollParents) {
        if (m_hasScrollParent)
            json->setBoolean("hasScrollParent", true);
        if (m_hasClipParent)
            json->setBoolean("hasClipParent", true);
    }

    if (flags & (LayerTreeIncludesDebugInfo | LayerTreeIncludesCompositingReasons)) {
        bool debug = flags & LayerTreeIncludesDebugInfo;
        RefPtr<JSONArray> compositingReasonsJSON = JSONArray::create();
        for (size_t i = 0; i < kNumberOfCompositingReasons; ++i) {
            if (m_debugInfo.getCompositingReasons() & kCompositingReasonStringMap[i].reason)
                compositingReasonsJSON->pushString(debug ? kCompositingReasonStringMap[i].description : kCompositingReasonStringMap[i].shortName);
        }
        json->setArray("compositingReasons", compositingReasonsJSON);

        RefPtr<JSONArray> squashingDisallowedReasonsJSON = JSONArray::create();
        for (size_t i = 0; i < kNumberOfSquashingDisallowedReasons; ++i) {
            if (m_debugInfo.getSquashingDisallowedReasons() & kSquashingDisallowedReasonStringMap[i].reason)
                squashingDisallowedReasonsJSON->pushString(debug ? kSquashingDisallowedReasonStringMap[i].description : kSquashingDisallowedReasonStringMap[i].shortName);
        }
        json->setArray("squashingDisallowedReasons", squashingDisallowedReasonsJSON);
    }

    if (m_children.size()) {
        RefPtr<JSONArray> childrenJSON = JSONArray::create();
        for (size_t i = 0; i < m_children.size(); i++)
            childrenJSON->pushObject(m_children[i]->layerTreeAsJSONInternal(flags, renderingContextMap));
        json->setArray("children", childrenJSON);
    }

    return json;
}

String GraphicsLayer::layerTreeAsText(LayerTreeFlags flags) const
{
    return layerTreeAsJSON(flags)->toPrettyJSONString();
}

static const cc::Layer* ccLayerForWebLayer(const WebLayer* webLayer)
{
    return webLayer ? webLayer->ccLayer() : nullptr;
}

String GraphicsLayer::debugName(cc::Layer* layer) const
{
    String name;
    if (!m_client)
        return name;

    String highlightDebugName;
    for (size_t i = 0; i < m_linkHighlights.size(); ++i) {
        if (layer == ccLayerForWebLayer(m_linkHighlights[i]->layer())) {
            highlightDebugName = "LinkHighlight[" + String::number(i) + "] for " + m_client->debugName(this);
            break;
        }
    }

    if (layer->id() == m_contentsLayerId) {
        name = "ContentsLayer for " + m_client->debugName(this);
    } else if (!highlightDebugName.isEmpty()) {
        name = highlightDebugName;
    } else if (layer == ccLayerForWebLayer(m_layer->layer())) {
        name = m_client->debugName(this);
    } else {
        ASSERT_NOT_REACHED();
    }
    return name;
}

void GraphicsLayer::setCompositingReasons(CompositingReasons reasons)
{
    m_debugInfo.setCompositingReasons(reasons);
}

void GraphicsLayer::setSquashingDisallowedReasons(SquashingDisallowedReasons reasons)
{
    m_debugInfo.setSquashingDisallowedReasons(reasons);
}

void GraphicsLayer::setOwnerNodeId(int nodeId)
{
    m_debugInfo.setOwnerNodeId(nodeId);
}

void GraphicsLayer::setPosition(const FloatPoint& point)
{
    m_position = point;
    platformLayer()->setPosition(m_position);
}

void GraphicsLayer::setSize(const FloatSize& size)
{
    // We are receiving negative sizes here that cause assertions to fail in the compositor. Clamp them to 0 to
    // avoid those assertions.
    // FIXME: This should be an ASSERT instead, as negative sizes should not exist in WebCore.
    FloatSize clampedSize = size;
    if (clampedSize.width() < 0 || clampedSize.height() < 0)
        clampedSize = FloatSize();

    if (clampedSize == m_size)
        return;

    m_size = clampedSize;

    m_layer->layer()->setBounds(flooredIntSize(m_size));
    // Note that we don't resize m_contentsLayer. It's up the caller to do that.

#ifndef NDEBUG
    // The red debug fill and needs to be invalidated if the layer resizes.
    setDisplayItemsUncached();
#endif
}

void GraphicsLayer::setTransform(const TransformationMatrix& transform)
{
    m_transform = transform;
    platformLayer()->setTransform(TransformationMatrix::toSkMatrix44(m_transform));
}

void GraphicsLayer::setTransformOrigin(const FloatPoint3D& transformOrigin)
{
    m_hasTransformOrigin = true;
    m_transformOrigin = transformOrigin;
    platformLayer()->setTransformOrigin(transformOrigin);
}

void GraphicsLayer::setShouldFlattenTransform(bool shouldFlatten)
{
    if (shouldFlatten == m_shouldFlattenTransform)
        return;

    m_shouldFlattenTransform = shouldFlatten;

    m_layer->layer()->setShouldFlattenTransform(shouldFlatten);
}

void GraphicsLayer::setRenderingContext(int context)
{
    if (m_3dRenderingContext == context)
        return;

    m_3dRenderingContext = context;
    m_layer->layer()->setRenderingContext(context);

    if (m_contentsLayer)
        m_contentsLayer->setRenderingContext(m_3dRenderingContext);
}

void GraphicsLayer::setMasksToBounds(bool masksToBounds)
{
    m_masksToBounds = masksToBounds;
    m_layer->layer()->setMasksToBounds(m_masksToBounds);
}

void GraphicsLayer::setDrawsContent(bool drawsContent)
{
    // Note carefully this early-exit is only correct because we also properly call
    // WebLayer::setDrawsContent whenever m_contentsLayer is set to a new layer in setupContentsLayer().
    if (drawsContent == m_drawsContent)
        return;

    m_drawsContent = drawsContent;
    updateLayerIsDrawable();

    if (!drawsContent && m_paintController)
        m_paintController.reset();
}

void GraphicsLayer::setContentsVisible(bool contentsVisible)
{
    // Note carefully this early-exit is only correct because we also properly call
    // WebLayer::setDrawsContent whenever m_contentsLayer is set to a new layer in setupContentsLayer().
    if (contentsVisible == m_contentsVisible)
        return;

    m_contentsVisible = contentsVisible;
    updateLayerIsDrawable();
}

void GraphicsLayer::setClipParent(WebLayer* parent)
{
    m_hasClipParent = !!parent;
    m_layer->layer()->setClipParent(parent);
}

void GraphicsLayer::setScrollParent(WebLayer* parent)
{
    m_hasScrollParent = !!parent;
    m_layer->layer()->setScrollParent(parent);
}

void GraphicsLayer::setBackgroundColor(const Color& color)
{
    if (color == m_backgroundColor)
        return;

    m_backgroundColor = color;
    m_layer->layer()->setBackgroundColor(m_backgroundColor.rgb());
}

void GraphicsLayer::setContentsOpaque(bool opaque)
{
    m_contentsOpaque = opaque;
    m_layer->layer()->setOpaque(m_contentsOpaque);
    clearContentsLayerIfUnregistered();
    if (m_contentsLayer)
        m_contentsLayer->setOpaque(opaque);
}

void GraphicsLayer::setMaskLayer(GraphicsLayer* maskLayer)
{
    if (maskLayer == m_maskLayer)
        return;

    m_maskLayer = maskLayer;
    WebLayer* maskWebLayer = m_maskLayer ? m_maskLayer->platformLayer() : 0;
    m_layer->layer()->setMaskLayer(maskWebLayer);
}

void GraphicsLayer::setContentsClippingMaskLayer(GraphicsLayer* contentsClippingMaskLayer)
{
    if (contentsClippingMaskLayer == m_contentsClippingMaskLayer)
        return;

    m_contentsClippingMaskLayer = contentsClippingMaskLayer;
    WebLayer* contentsLayer = contentsLayerIfRegistered();
    if (!contentsLayer)
        return;
    WebLayer* contentsClippingMaskWebLayer = m_contentsClippingMaskLayer ? m_contentsClippingMaskLayer->platformLayer() : 0;
    contentsLayer->setMaskLayer(contentsClippingMaskWebLayer);
    updateContentsRect();
}

void GraphicsLayer::setBackfaceVisibility(bool visible)
{
    m_backfaceVisibility = visible;
    platformLayer()->setDoubleSided(m_backfaceVisibility);
}

void GraphicsLayer::setOpacity(float opacity)
{
    float clampedOpacity = clampTo(opacity, 0.0f, 1.0f);
    m_opacity = clampedOpacity;
    platformLayer()->setOpacity(opacity);
}

void GraphicsLayer::setBlendMode(WebBlendMode blendMode)
{
    if (m_blendMode == blendMode)
        return;
    m_blendMode = blendMode;
    platformLayer()->setBlendMode(blendMode);
}

void GraphicsLayer::setIsRootForIsolatedGroup(bool isolated)
{
    if (m_isRootForIsolatedGroup == isolated)
        return;
    m_isRootForIsolatedGroup = isolated;
    platformLayer()->setIsRootForIsolatedGroup(isolated);
}

void GraphicsLayer::setContentsNeedsDisplay()
{
    if (WebLayer* contentsLayer = contentsLayerIfRegistered()) {
        contentsLayer->invalidate();
        trackPaintInvalidation(*this, m_contentsRect, PaintInvalidationFull);
    }
}

void GraphicsLayer::setNeedsDisplay()
{
    if (!drawsContent())
        return;

    // TODO(chrishtr): stop invalidating the rects once FrameView::paintRecursively does so.
    m_layer->layer()->invalidate();
    for (size_t i = 0; i < m_linkHighlights.size(); ++i)
        m_linkHighlights[i]->invalidate();
    getPaintController().invalidateAll();

    trackPaintInvalidation(*this, IntRect(IntPoint(), expandedIntSize(m_size)), PaintInvalidationFull);
}

void GraphicsLayer::setNeedsDisplayInRect(const IntRect& rect, PaintInvalidationReason invalidationReason, const DisplayItemClient& client)
{
    if (!drawsContent())
        return;

    m_layer->layer()->invalidateRect(rect);
    if (firstPaintInvalidationTrackingEnabled())
        m_debugInfo.appendAnnotatedInvalidateRect(rect, invalidationReason);
    for (size_t i = 0; i < m_linkHighlights.size(); ++i)
        m_linkHighlights[i]->invalidate();

    trackPaintInvalidation(client, rect, invalidationReason);
}

void GraphicsLayer::setContentsRect(const IntRect& rect)
{
    if (rect == m_contentsRect)
        return;

    m_contentsRect = rect;
    updateContentsRect();
}

void GraphicsLayer::setContentsToImage(Image* image, RespectImageOrientationEnum respectImageOrientation)
{
    RefPtr<SkImage> skImage = image ? image->imageForCurrentFrame() : nullptr;

    if (image && skImage && image->isBitmapImage()) {
        if (respectImageOrientation == RespectImageOrientation) {
            ImageOrientation imageOrientation = toBitmapImage(image)->currentFrameOrientation();
            skImage = DragImage::resizeAndOrientImage(skImage.release(), imageOrientation);
        }
    }

    if (image && skImage) {
        if (!m_imageLayer) {
            m_imageLayer = wrapUnique(Platform::current()->compositorSupport()->createImageLayer());
            registerContentsLayer(m_imageLayer->layer());
        }
        m_imageLayer->setImage(skImage.get());
        m_imageLayer->layer()->setOpaque(image->currentFrameKnownToBeOpaque());
        updateContentsRect();
    } else {
        if (m_imageLayer) {
            unregisterContentsLayer(m_imageLayer->layer());
            m_imageLayer.reset();
        }
    }

    setContentsTo(m_imageLayer ? m_imageLayer->layer() : 0);
}

WebLayer* GraphicsLayer::platformLayer() const
{
    return m_layer->layer();
}

void GraphicsLayer::setFilters(const FilterOperations& filters)
{
    std::unique_ptr<CompositorFilterOperations> compositorFilters = CompositorFilterOperations::create();
    SkiaImageFilterBuilder::buildFilterOperations(filters, compositorFilters.get());
    m_layer->layer()->setFilters(compositorFilters->asFilterOperations());
}

void GraphicsLayer::setBackdropFilters(const FilterOperations& filters)
{
    std::unique_ptr<CompositorFilterOperations> compositorFilters = CompositorFilterOperations::create();
    SkiaImageFilterBuilder::buildFilterOperations(filters, compositorFilters.get());
    m_layer->layer()->setBackgroundFilters(compositorFilters->asFilterOperations());
}

void GraphicsLayer::setFilterQuality(SkFilterQuality filterQuality)
{
    if (m_imageLayer)
        m_imageLayer->setNearestNeighbor(filterQuality == kNone_SkFilterQuality);
}

void GraphicsLayer::setPaintingPhase(GraphicsLayerPaintingPhase phase)
{
    if (m_paintingPhase == phase)
        return;
    m_paintingPhase = phase;
    setNeedsDisplay();
}

void GraphicsLayer::addLinkHighlight(LinkHighlight* linkHighlight)
{
    ASSERT(linkHighlight && !m_linkHighlights.contains(linkHighlight));
    m_linkHighlights.append(linkHighlight);
    linkHighlight->layer()->setLayerClient(this);
    updateChildList();
}

void GraphicsLayer::removeLinkHighlight(LinkHighlight* linkHighlight)
{
    m_linkHighlights.remove(m_linkHighlights.find(linkHighlight));
    updateChildList();
}

void GraphicsLayer::setScrollableArea(ScrollableArea* scrollableArea, bool isViewport)
{
    if (m_scrollableArea == scrollableArea)
        return;

    m_scrollableArea = scrollableArea;

    // Viewport scrolling may involve pinch zoom and gets routed through
    // WebViewImpl explicitly rather than via GraphicsLayer::didScroll.
    if (isViewport)
        m_layer->layer()->setScrollClient(0);
    else
        m_layer->layer()->setScrollClient(this);
}

void GraphicsLayer::didScroll()
{
    if (m_scrollableArea) {
        DoublePoint newPosition = m_scrollableArea->minimumScrollPosition() + toDoubleSize(m_layer->layer()->scrollPositionDouble());

        // FrameView::setScrollPosition doesn't work for compositor commits (interacts poorly with programmatic scroll animations)
        // so we need to use the ScrollableArea version. The FrameView method should go away soon anyway.
        m_scrollableArea->ScrollableArea::setScrollPosition(newPosition, CompositorScroll);
    }
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat> GraphicsLayer::TakeDebugInfo(cc::Layer* layer)
{
    std::unique_ptr<base::trace_event::TracedValue> tracedValue(m_debugInfo.asTracedValue());
    tracedValue->SetString("layer_name", WTF::StringUTF8Adaptor(debugName(layer)).asStringPiece());
    return std::move(tracedValue);
}

PaintController& GraphicsLayer::getPaintController()
{
    RELEASE_ASSERT(drawsContent());
    if (!m_paintController)
        m_paintController = PaintController::create();
    return *m_paintController;
}

void GraphicsLayer::setElementId(const CompositorElementId& id)
{
    if (WebLayer* layer = platformLayer())
        layer->setElementId(id);
}

void GraphicsLayer::setCompositorMutableProperties(uint32_t properties)
{
    if (WebLayer* layer = platformLayer())
        layer->setCompositorMutableProperties(properties);
}

#if DCHECK_IS_ON()

PassRefPtr<SkPicture> GraphicsLayer::capturePicture()
{
    if (!drawsContent())
        return nullptr;

    IntSize intSize = expandedIntSize(size());
    GraphicsContext graphicsContext(getPaintController());
    graphicsContext.beginRecording(IntRect(IntPoint(0, 0), intSize));
    getPaintController().paintArtifact().replay(graphicsContext);
    return graphicsContext.endRecording();
}

static bool pixelComponentsDiffer(int c1, int c2)
{
    // Compare strictly for saturated values.
    if (c1 == 0 || c1 == 255 || c2 == 0 || c2 == 255)
        return c1 != c2;
    // Tolerate invisible differences that may occur in gradients etc.
    return abs(c1 - c2) > 2;
}

static bool pixelsDiffer(SkColor p1, SkColor p2)
{
    return pixelComponentsDiffer(SkColorGetA(p1), SkColorGetA(p2))
        || pixelComponentsDiffer(SkColorGetR(p1), SkColorGetR(p2))
        || pixelComponentsDiffer(SkColorGetG(p1), SkColorGetG(p2))
        || pixelComponentsDiffer(SkColorGetB(p1), SkColorGetB(p2));
}

void GraphicsLayer::checkPaintUnderInvalidations(const SkPicture& newPicture)
{
    if (!drawsContent())
        return;

    auto it = paintInvalidationTrackingMap().find(this);
    if (it == paintInvalidationTrackingMap().end())
        return;
    PaintInvalidationTracking& tracking = it->value;
    if (!tracking.lastPaintedPicture)
        return;

    SkBitmap oldBitmap;
    int width = static_cast<int>(ceilf(std::min(tracking.lastPaintedPicture->cullRect().width(), newPicture.cullRect().width())));
    int height = static_cast<int>(ceilf(std::min(tracking.lastPaintedPicture->cullRect().height(), newPicture.cullRect().height())));
    oldBitmap.allocPixels(SkImageInfo::MakeN32Premul(width, height));
    SkCanvas(oldBitmap).drawPicture(tracking.lastPaintedPicture.get());

    SkBitmap newBitmap;
    newBitmap.allocPixels(SkImageInfo::MakeN32Premul(width, height));
    SkCanvas(newBitmap).drawPicture(&newPicture);

    oldBitmap.lockPixels();
    newBitmap.lockPixels();
    int mismatchingPixels = 0;
    static const int maxMismatchesToReport = 50;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            SkColor oldPixel = oldBitmap.getColor(x, y);
            SkColor newPixel = newBitmap.getColor(x, y);
            if (pixelsDiffer(oldPixel, newPixel) && !tracking.paintInvalidationRegionSinceLastPaint.contains(IntPoint(x, y))) {
                if (mismatchingPixels < maxMismatchesToReport) {
                    UnderPaintInvalidation underPaintInvalidation = { x, y, oldPixel, newPixel };
                    tracking.underPaintInvalidations.append(underPaintInvalidation);
                    LOG(ERROR) << debugName() << " Uninvalidated old/new pixels mismatch at " << x << "," << y << " old:" << std::hex << oldPixel << " new:" << newPixel;
                } else if (mismatchingPixels == maxMismatchesToReport) {
                    LOG(ERROR) << "and more...";
                }
                ++mismatchingPixels;
                *newBitmap.getAddr32(x, y) = SK_ColorRED;
            } else {
                *newBitmap.getAddr32(x, y) = SK_ColorTRANSPARENT;
            }
        }
    }

    oldBitmap.unlockPixels();
    newBitmap.unlockPixels();

    // Visualize under-invalidations by overlaying the new bitmap (containing red pixels indicating under-invalidations,
    // and transparent pixels otherwise) onto the painting.
    SkPictureRecorder recorder;
    recorder.beginRecording(width, height);
    recorder.getRecordingCanvas()->drawBitmap(newBitmap, 0, 0);
    RefPtr<SkPicture> picture = fromSkSp(recorder.finishRecordingAsPicture());
    getPaintController().appendDebugDrawingAfterCommit(*this, picture, offsetFromLayoutObjectWithSubpixelAccumulation());
}

#endif // DCHECK_IS_ON()

} // namespace blink

#ifndef NDEBUG
void showGraphicsLayerTree(const blink::GraphicsLayer* layer)
{
    if (!layer) {
        fprintf(stderr, "Cannot showGraphicsLayerTree for (nil).\n");
        return;
    }

    String output = layer->layerTreeAsText(blink::LayerTreeIncludesDebugInfo);
    fprintf(stderr, "%s\n", output.utf8().data());
}
#endif
