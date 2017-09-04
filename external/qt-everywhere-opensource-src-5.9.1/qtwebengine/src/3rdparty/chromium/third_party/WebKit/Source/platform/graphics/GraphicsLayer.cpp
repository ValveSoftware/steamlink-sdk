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
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/geometry/Region.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/CompositorFilterOperations.h"
#include "platform/graphics/FirstPaintInvalidationTracking.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/LinkHighlight.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/RasterInvalidationTracking.h"
#include "platform/json/JSONValues.h"
#include "platform/scroll/ScrollableArea.h"
#include "platform/text/TextStream.h"
#include "platform/tracing/TraceEvent.h"
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

template class RasterInvalidationTrackingMap<const GraphicsLayer>;
static RasterInvalidationTrackingMap<const GraphicsLayer>&
rasterInvalidationTrackingMap() {
  DEFINE_STATIC_LOCAL(RasterInvalidationTrackingMap<const GraphicsLayer>, map,
                      ());
  return map;
}

std::unique_ptr<GraphicsLayer> GraphicsLayer::create(
    GraphicsLayerClient* client) {
  return wrapUnique(new GraphicsLayer(client));
}

GraphicsLayer::GraphicsLayer(GraphicsLayerClient* client)
    : m_client(client),
      m_backgroundColor(Color::transparent),
      m_opacity(1),
      m_blendMode(WebBlendModeNormal),
      m_hasTransformOrigin(false),
      m_contentsOpaque(false),
      m_shouldFlattenTransform(true),
      m_backfaceVisibility(true),
      m_drawsContent(false),
      m_contentsVisible(true),
      m_isRootForIsolatedGroup(false),
      m_hasScrollParent(false),
      m_hasClipParent(false),
      m_painted(false),
      m_isTrackingRasterInvalidations(client &&
                                      client->isTrackingRasterInvalidations()),
      m_paintingPhase(GraphicsLayerPaintAllWithOverflowClip),
      m_parent(0),
      m_maskLayer(0),
      m_contentsClippingMaskLayer(0),
      m_paintCount(0),
      m_contentsLayer(0),
      m_contentsLayerId(0),
      m_scrollableArea(nullptr),
      m_renderingContext3d(0),
      m_hasPreferredRasterBounds(false) {
#if ENABLE(ASSERT)
  if (m_client)
    m_client->verifyNotPainting();
#endif

  m_contentLayerDelegate = makeUnique<ContentLayerDelegate>(this);
  m_layer = Platform::current()->compositorSupport()->createContentLayer(
      m_contentLayerDelegate.get());
  m_layer->layer()->setDrawsContent(m_drawsContent && m_contentsVisible);
  m_layer->layer()->setLayerClient(this);
}

GraphicsLayer::~GraphicsLayer() {
  for (size_t i = 0; i < m_linkHighlights.size(); ++i)
    m_linkHighlights[i]->clearCurrentGraphicsLayer();
  m_linkHighlights.clear();

#if ENABLE(ASSERT)
  if (m_client)
    m_client->verifyNotPainting();
#endif

  removeAllChildren();
  removeFromParent();

  rasterInvalidationTrackingMap().remove(this);
  ASSERT(!m_parent);
}

LayoutRect GraphicsLayer::visualRect() const {
  LayoutRect bounds = LayoutRect(FloatPoint(), size());
  bounds.move(offsetFromLayoutObjectWithSubpixelAccumulation());
  return bounds;
}

void GraphicsLayer::setHasWillChangeTransformHint(bool hasWillChangeTransform) {
  m_layer->layer()->setHasWillChangeTransformHint(hasWillChangeTransform);
}

void GraphicsLayer::setPreferredRasterBounds(const IntSize& bounds) {
  m_preferredRasterBounds = bounds;
  m_hasPreferredRasterBounds = true;
  m_layer->layer()->setPreferredRasterBounds(bounds);
}

void GraphicsLayer::clearPreferredRasterBounds() {
  m_preferredRasterBounds = IntSize();
  m_hasPreferredRasterBounds = false;
  m_layer->layer()->clearPreferredRasterBounds();
}

void GraphicsLayer::setParent(GraphicsLayer* layer) {
  ASSERT(!layer || !layer->hasAncestor(this));
  m_parent = layer;
}

#if ENABLE(ASSERT)

bool GraphicsLayer::hasAncestor(GraphicsLayer* ancestor) const {
  for (GraphicsLayer* curr = parent(); curr; curr = curr->parent()) {
    if (curr == ancestor)
      return true;
  }

  return false;
}

#endif

bool GraphicsLayer::setChildren(const GraphicsLayerVector& newChildren) {
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

void GraphicsLayer::addChildInternal(GraphicsLayer* childLayer) {
  ASSERT(childLayer != this);

  if (childLayer->parent())
    childLayer->removeFromParent();

  childLayer->setParent(this);
  m_children.append(childLayer);

  // Don't call updateChildList here, this function is used in cases where it
  // should not be called until all children are processed.
}

void GraphicsLayer::addChild(GraphicsLayer* childLayer) {
  addChildInternal(childLayer);
  updateChildList();
}

void GraphicsLayer::addChildBelow(GraphicsLayer* childLayer,
                                  GraphicsLayer* sibling) {
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

void GraphicsLayer::removeAllChildren() {
  while (!m_children.isEmpty()) {
    GraphicsLayer* curLayer = m_children.last();
    ASSERT(curLayer->parent());
    curLayer->removeFromParent();
  }
}

void GraphicsLayer::removeFromParent() {
  if (m_parent) {
    // We use reverseFind so that removeAllChildren() isn't n^2.
    m_parent->m_children.remove(m_parent->m_children.reverseFind(this));
    setParent(0);
  }

  platformLayer()->removeFromParent();
}

void GraphicsLayer::setOffsetFromLayoutObject(
    const IntSize& offset,
    ShouldSetNeedsDisplay shouldSetNeedsDisplay) {
  setOffsetDoubleFromLayoutObject(offset);
}

void GraphicsLayer::setOffsetDoubleFromLayoutObject(
    const DoubleSize& offset,
    ShouldSetNeedsDisplay shouldSetNeedsDisplay) {
  if (offset == m_offsetFromLayoutObject)
    return;

  m_offsetFromLayoutObject = offset;
  platformLayer()->setFiltersOrigin(FloatPoint() - toFloatSize(offset));

  // If the compositing layer offset changes, we need to repaint.
  if (shouldSetNeedsDisplay == SetNeedsDisplay)
    setNeedsDisplay();
}

LayoutSize GraphicsLayer::offsetFromLayoutObjectWithSubpixelAccumulation()
    const {
  return LayoutSize(offsetFromLayoutObject()) +
         client()->subpixelAccumulation();
}

IntRect GraphicsLayer::interestRect() {
  return m_previousInterestRect;
}

void GraphicsLayer::paint(const IntRect* interestRect,
                          GraphicsContext::DisabledMode disabledMode) {
  if (paintWithoutCommit(interestRect, disabledMode)) {
    getPaintController().commitNewDisplayItems(
        offsetFromLayoutObjectWithSubpixelAccumulation());
    if (RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled()) {
      sk_sp<SkPicture> newPicture = capturePicture();
      checkPaintUnderInvalidations(*newPicture);
      RasterInvalidationTracking& tracking =
          rasterInvalidationTrackingMap().add(this);
      tracking.lastPaintedPicture = std::move(newPicture);
      tracking.lastInterestRect = m_previousInterestRect;
      tracking.rasterInvalidationRegionSinceLastPaint = Region();
    }
  }
}

bool GraphicsLayer::paintWithoutCommit(
    const IntRect* interestRect,
    GraphicsContext::DisabledMode disabledMode) {
  ASSERT(drawsContent());

  if (!m_client)
    return false;
  if (firstPaintInvalidationTrackingEnabled())
    m_debugInfo.clearAnnotatedInvalidateRects();
  incrementPaintCount();

  IntRect newInterestRect;
  if (!interestRect) {
    newInterestRect =
        m_client->computeInterestRect(this, m_previousInterestRect);
    interestRect = &newInterestRect;
  }

  if (!getPaintController().subsequenceCachingIsDisabled() &&
      !m_client->needsRepaint(*this) && !getPaintController().cacheIsEmpty() &&
      m_previousInterestRect == *interestRect) {
    return false;
  }

  GraphicsContext context(getPaintController(), disabledMode);

  m_previousInterestRect = *interestRect;
  m_client->paintContents(this, context, m_paintingPhase, *interestRect);
  notifyFirstPaintToClient();
  return true;
}

void GraphicsLayer::notifyFirstPaintToClient() {
  bool isFirstPaint = false;
  if (!m_painted) {
    DisplayItemList& itemList = m_paintController->newDisplayItemList();
    for (DisplayItem& item : itemList) {
      DisplayItem::Type type = item.getType();
      if (type == DisplayItem::kDocumentBackground &&
          !m_paintController->nonDefaultBackgroundColorPainted()) {
        continue;
      }
      if (DisplayItem::isDrawingType(type) &&
          static_cast<const DrawingDisplayItem&>(item).picture()) {
        m_painted = true;
        isFirstPaint = true;
        break;
      }
    }
  }
  m_client->notifyPaint(isFirstPaint, m_paintController->textPainted(),
                        m_paintController->imagePainted());
}

void GraphicsLayer::updateChildList() {
  WebLayer* childHost = m_layer->layer();
  childHost->removeAllChildren();

  clearContentsLayerIfUnregistered();

  if (m_contentsLayer) {
    // FIXME: Add the contents layer in the correct order with negative z-order
    // children. This does not currently cause visible rendering issues because
    // contents layers are only used for replaced elements that don't have
    // children.
    childHost->addChild(m_contentsLayer);
  }

  for (size_t i = 0; i < m_children.size(); ++i)
    childHost->addChild(m_children[i]->platformLayer());

  for (size_t i = 0; i < m_linkHighlights.size(); ++i)
    childHost->addChild(m_linkHighlights[i]->layer());
}

void GraphicsLayer::updateLayerIsDrawable() {
  // For the rest of the accelerated compositor code, there is no reason to make
  // a distinction between drawsContent and contentsVisible. So, for
  // m_layer->layer(), these two flags are combined here. |m_contentsLayer|
  // shouldn't receive the drawsContent flag, so it is only given
  // contentsVisible.

  m_layer->layer()->setDrawsContent(m_drawsContent && m_contentsVisible);
  if (WebLayer* contentsLayer = contentsLayerIfRegistered())
    contentsLayer->setDrawsContent(m_contentsVisible);

  if (m_drawsContent) {
    m_layer->layer()->invalidate();
    for (size_t i = 0; i < m_linkHighlights.size(); ++i)
      m_linkHighlights[i]->invalidate();
  }
}

void GraphicsLayer::updateContentsRect() {
  WebLayer* contentsLayer = contentsLayerIfRegistered();
  if (!contentsLayer)
    return;

  contentsLayer->setPosition(
      FloatPoint(m_contentsRect.x(), m_contentsRect.y()));
  contentsLayer->setBounds(
      IntSize(m_contentsRect.width(), m_contentsRect.height()));

  if (m_contentsClippingMaskLayer) {
    if (m_contentsClippingMaskLayer->size() != m_contentsRect.size()) {
      m_contentsClippingMaskLayer->setSize(FloatSize(m_contentsRect.size()));
      m_contentsClippingMaskLayer->setNeedsDisplay();
    }
    m_contentsClippingMaskLayer->setPosition(FloatPoint());
    m_contentsClippingMaskLayer->setOffsetFromLayoutObject(
        offsetFromLayoutObject() +
        IntSize(m_contentsRect.location().x(), m_contentsRect.location().y()));
  }
}

static HashSet<int>* s_registeredLayerSet;

void GraphicsLayer::registerContentsLayer(WebLayer* layer) {
  if (!s_registeredLayerSet)
    s_registeredLayerSet = new HashSet<int>;
  if (s_registeredLayerSet->contains(layer->id()))
    CRASH();
  s_registeredLayerSet->add(layer->id());
}

void GraphicsLayer::unregisterContentsLayer(WebLayer* layer) {
  ASSERT(s_registeredLayerSet);
  if (!s_registeredLayerSet->contains(layer->id()))
    CRASH();
  s_registeredLayerSet->remove(layer->id());
}

void GraphicsLayer::setContentsTo(WebLayer* layer) {
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

void GraphicsLayer::setupContentsLayer(WebLayer* contentsLayer) {
  ASSERT(contentsLayer);
  m_contentsLayer = contentsLayer;
  m_contentsLayerId = m_contentsLayer->id();

  m_contentsLayer->setLayerClient(this);
  m_contentsLayer->setTransformOrigin(FloatPoint3D());
  m_contentsLayer->setUseParentBackfaceVisibility(true);

  // It is necessary to call setDrawsContent as soon as we receive the new
  // contentsLayer, for the correctness of early exit conditions in
  // setDrawsContent() and setContentsVisible().
  m_contentsLayer->setDrawsContent(m_contentsVisible);

  // Insert the content layer first. Video elements require this, because they
  // have shadow content that must display in front of the video.
  m_layer->layer()->insertChild(m_contentsLayer, 0);
  WebLayer* borderWebLayer = m_contentsClippingMaskLayer
                                 ? m_contentsClippingMaskLayer->platformLayer()
                                 : 0;
  m_contentsLayer->setMaskLayer(borderWebLayer);

  m_contentsLayer->setRenderingContext(m_renderingContext3d);
}

void GraphicsLayer::clearContentsLayerIfUnregistered() {
  if (!m_contentsLayerId || s_registeredLayerSet->contains(m_contentsLayerId))
    return;

  m_contentsLayer = 0;
  m_contentsLayerId = 0;
}

GraphicsLayerDebugInfo& GraphicsLayer::debugInfo() {
  return m_debugInfo;
}

WebLayer* GraphicsLayer::contentsLayerIfRegistered() {
  clearContentsLayerIfUnregistered();
  return m_contentsLayer;
}

void GraphicsLayer::setTracksRasterInvalidations(
    bool tracksRasterInvalidations) {
  resetTrackedRasterInvalidations();
  m_isTrackingRasterInvalidations = tracksRasterInvalidations;
}

void GraphicsLayer::resetTrackedRasterInvalidations() {
  RasterInvalidationTracking* tracking =
      rasterInvalidationTrackingMap().find(this);
  if (!tracking)
    return;

  if (RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled())
    tracking->trackedRasterInvalidations.clear();
  else
    rasterInvalidationTrackingMap().remove(this);
}

bool GraphicsLayer::hasTrackedRasterInvalidations() const {
  if (auto* tracking = getRasterInvalidationTracking())
    return !tracking->trackedRasterInvalidations.isEmpty();
  return false;
}

const RasterInvalidationTracking* GraphicsLayer::getRasterInvalidationTracking()
    const {
  return rasterInvalidationTrackingMap().find(this);
}

void GraphicsLayer::trackRasterInvalidation(const DisplayItemClient& client,
                                            const IntRect& rect,
                                            PaintInvalidationReason reason) {
  if (!isTrackingOrCheckingRasterInvalidations() || rect.isEmpty())
    return;

  RasterInvalidationTracking& tracking =
      rasterInvalidationTrackingMap().add(this);

  if (m_isTrackingRasterInvalidations) {
    RasterInvalidationInfo info;
    info.client = &client;
    info.clientDebugName = client.debugName();
    info.rect = rect;
    info.reason = reason;
    tracking.trackedRasterInvalidations.append(info);
  }

  if (RuntimeEnabledFeatures::paintUnderInvalidationCheckingEnabled()) {
    // TODO(crbug.com/496260): Some antialiasing effects overflow the paint
    // invalidation rect.
    IntRect r = rect;
    r.inflate(1);
    tracking.rasterInvalidationRegionSinceLastPaint.unite(r);
  }
}

template <typename T>
static std::unique_ptr<JSONArray> pointAsJSONArray(const T& point) {
  std::unique_ptr<JSONArray> array = JSONArray::create();
  array->pushDouble(point.x());
  array->pushDouble(point.y());
  return array;
}

template <typename T>
static std::unique_ptr<JSONArray> sizeAsJSONArray(const T& size) {
  std::unique_ptr<JSONArray> array = JSONArray::create();
  array->pushDouble(size.width());
  array->pushDouble(size.height());
  return array;
}

static double roundCloseToZero(double number) {
  return std::abs(number) < 1e-7 ? 0 : number;
}

static std::unique_ptr<JSONArray> transformAsJSONArray(
    const TransformationMatrix& t) {
  std::unique_ptr<JSONArray> array = JSONArray::create();
  {
    std::unique_ptr<JSONArray> row = JSONArray::create();
    row->pushDouble(roundCloseToZero(t.m11()));
    row->pushDouble(roundCloseToZero(t.m12()));
    row->pushDouble(roundCloseToZero(t.m13()));
    row->pushDouble(roundCloseToZero(t.m14()));
    array->pushArray(std::move(row));
  }
  {
    std::unique_ptr<JSONArray> row = JSONArray::create();
    row->pushDouble(roundCloseToZero(t.m21()));
    row->pushDouble(roundCloseToZero(t.m22()));
    row->pushDouble(roundCloseToZero(t.m23()));
    row->pushDouble(roundCloseToZero(t.m24()));
    array->pushArray(std::move(row));
  }
  {
    std::unique_ptr<JSONArray> row = JSONArray::create();
    row->pushDouble(roundCloseToZero(t.m31()));
    row->pushDouble(roundCloseToZero(t.m32()));
    row->pushDouble(roundCloseToZero(t.m33()));
    row->pushDouble(roundCloseToZero(t.m34()));
    array->pushArray(std::move(row));
  }
  {
    std::unique_ptr<JSONArray> row = JSONArray::create();
    row->pushDouble(roundCloseToZero(t.m41()));
    row->pushDouble(roundCloseToZero(t.m42()));
    row->pushDouble(roundCloseToZero(t.m43()));
    row->pushDouble(roundCloseToZero(t.m44()));
    array->pushArray(std::move(row));
  }
  return array;
}

static String pointerAsString(const void* ptr) {
  TextStream ts;
  ts << ptr;
  return ts.release();
}

std::unique_ptr<JSONObject> GraphicsLayer::layerTreeAsJSON(
    LayerTreeFlags flags) const {
  RenderingContextMap renderingContextMap;
  if (flags & OutputAsLayerTree)
    return layerTreeAsJSONInternal(flags, renderingContextMap);
  std::unique_ptr<JSONObject> json = JSONObject::create();
  std::unique_ptr<JSONArray> layersArray = JSONArray::create();
  for (auto& child : m_children)
    child->layersAsJSONArray(flags, renderingContextMap, layersArray.get());
  json->setArray("layers", std::move(layersArray));
  return json;
}

std::unique_ptr<JSONObject> GraphicsLayer::layerAsJSONInternal(
    LayerTreeFlags flags,
    RenderingContextMap& renderingContextMap) const {
  std::unique_ptr<JSONObject> json = JSONObject::create();

  if (flags & LayerTreeIncludesDebugInfo)
    json->setString("this", pointerAsString(this));

  json->setString("name", debugName());

  if (m_position != FloatPoint())
    json->setArray("position", pointAsJSONArray(m_position));

  if (m_hasTransformOrigin &&
      m_transformOrigin !=
          FloatPoint3D(m_size.width() * 0.5f, m_size.height() * 0.5f, 0))
    json->setArray("transformOrigin", pointAsJSONArray(m_transformOrigin));

  if (m_size != IntSize())
    json->setArray("bounds", sizeAsJSONArray(m_size));

  if (m_opacity != 1)
    json->setDouble("opacity", m_opacity);

  if (m_blendMode != WebBlendModeNormal)
    json->setString("blendMode",
                    compositeOperatorName(CompositeSourceOver, m_blendMode));

  if (m_isRootForIsolatedGroup)
    json->setBoolean("isolate", m_isRootForIsolatedGroup);

  if (m_contentsOpaque)
    json->setBoolean("contentsOpaque", m_contentsOpaque);

  if (!m_shouldFlattenTransform)
    json->setBoolean("shouldFlattenTransform", m_shouldFlattenTransform);

  if (m_renderingContext3d) {
    RenderingContextMap::const_iterator it =
        renderingContextMap.find(m_renderingContext3d);
    int contextId = renderingContextMap.size() + 1;
    if (it == renderingContextMap.end())
      renderingContextMap.set(m_renderingContext3d, contextId);
    else
      contextId = it->value;

    json->setInteger("3dRenderingContext", contextId);
  }

  if (m_drawsContent)
    json->setBoolean("drawsContent", m_drawsContent);

  if (!m_contentsVisible)
    json->setBoolean("contentsVisible", m_contentsVisible);

  if (!m_backfaceVisibility)
    json->setString("backfaceVisibility",
                    m_backfaceVisibility ? "visible" : "hidden");

  if (m_hasPreferredRasterBounds) {
    json->setArray("preferredRasterBounds",
                   sizeAsJSONArray(m_preferredRasterBounds));
  }

  if (flags & LayerTreeIncludesDebugInfo)
    json->setString("client", pointerAsString(m_client));

  if (m_backgroundColor.alpha())
    json->setString("backgroundColor",
                    m_backgroundColor.nameForLayoutTreeAsText());

  if (!m_transform.isIdentity())
    json->setArray("transform", transformAsJSONArray(m_transform));

  if (flags & LayerTreeIncludesPaintInvalidations)
    rasterInvalidationTrackingMap().asJSON(this, json.get());

  if ((flags & LayerTreeIncludesPaintingPhases) && m_paintingPhase) {
    std::unique_ptr<JSONArray> paintingPhasesJSON = JSONArray::create();
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
    json->setArray("paintingPhases", std::move(paintingPhasesJSON));
  }

  if (flags & LayerTreeIncludesClipAndScrollParents) {
    if (m_hasScrollParent)
      json->setBoolean("hasScrollParent", true);
    if (m_hasClipParent)
      json->setBoolean("hasClipParent", true);
  }

  if (flags &
      (LayerTreeIncludesDebugInfo | LayerTreeIncludesCompositingReasons)) {
    bool debug = flags & LayerTreeIncludesDebugInfo;
    std::unique_ptr<JSONArray> compositingReasonsJSON = JSONArray::create();
    for (size_t i = 0; i < kNumberOfCompositingReasons; ++i) {
      if (m_debugInfo.getCompositingReasons() &
          kCompositingReasonStringMap[i].reason)
        compositingReasonsJSON->pushString(
            debug ? kCompositingReasonStringMap[i].description
                  : kCompositingReasonStringMap[i].shortName);
    }
    json->setArray("compositingReasons", std::move(compositingReasonsJSON));

    std::unique_ptr<JSONArray> squashingDisallowedReasonsJSON =
        JSONArray::create();
    for (size_t i = 0; i < kNumberOfSquashingDisallowedReasons; ++i) {
      if (m_debugInfo.getSquashingDisallowedReasons() &
          kSquashingDisallowedReasonStringMap[i].reason)
        squashingDisallowedReasonsJSON->pushString(
            debug ? kSquashingDisallowedReasonStringMap[i].description
                  : kSquashingDisallowedReasonStringMap[i].shortName);
    }
    json->setArray("squashingDisallowedReasons",
                   std::move(squashingDisallowedReasonsJSON));
  }
  return json;
}

std::unique_ptr<JSONObject> GraphicsLayer::layerTreeAsJSONInternal(
    LayerTreeFlags flags,
    RenderingContextMap& renderingContextMap) const {
  std::unique_ptr<JSONObject> json =
      layerAsJSONInternal(flags, renderingContextMap);

  if (m_children.size()) {
    std::unique_ptr<JSONArray> childrenJSON = JSONArray::create();
    for (size_t i = 0; i < m_children.size(); i++)
      childrenJSON->pushObject(
          m_children[i]->layerTreeAsJSONInternal(flags, renderingContextMap));
    json->setArray("children", std::move(childrenJSON));
  }

  return json;
}

void GraphicsLayer::layersAsJSONArray(LayerTreeFlags flags,
                                      RenderingContextMap& renderingContextMap,
                                      JSONArray* jsonArray) const {
  jsonArray->pushObject(layerAsJSONInternal(flags, renderingContextMap));

  if (m_children.size()) {
    for (auto& child : m_children)
      child->layersAsJSONArray(flags, renderingContextMap, jsonArray);
  }
}

String GraphicsLayer::layerTreeAsText(LayerTreeFlags flags) const {
  return layerTreeAsJSON(flags)->toPrettyJSONString();
}

static const cc::Layer* ccLayerForWebLayer(const WebLayer* webLayer) {
  return webLayer ? webLayer->ccLayer() : nullptr;
}

String GraphicsLayer::debugName(cc::Layer* layer) const {
  String name;
  if (!m_client)
    return name;

  String highlightDebugName;
  for (size_t i = 0; i < m_linkHighlights.size(); ++i) {
    if (layer == ccLayerForWebLayer(m_linkHighlights[i]->layer())) {
      highlightDebugName = "LinkHighlight[" + String::number(i) + "] for " +
                           m_client->debugName(this);
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

void GraphicsLayer::setCompositingReasons(CompositingReasons reasons) {
  m_debugInfo.setCompositingReasons(reasons);
}

void GraphicsLayer::setSquashingDisallowedReasons(
    SquashingDisallowedReasons reasons) {
  m_debugInfo.setSquashingDisallowedReasons(reasons);
}

void GraphicsLayer::setOwnerNodeId(int nodeId) {
  m_debugInfo.setOwnerNodeId(nodeId);
}

void GraphicsLayer::setPosition(const FloatPoint& point) {
  m_position = point;
  platformLayer()->setPosition(m_position);
}

void GraphicsLayer::setSize(const FloatSize& size) {
  // We are receiving negative sizes here that cause assertions to fail in the
  // compositor. Clamp them to 0 to avoid those assertions.
  // FIXME: This should be an ASSERT instead, as negative sizes should not exist
  // in WebCore.
  FloatSize clampedSize = size;
  if (clampedSize.width() < 0 || clampedSize.height() < 0)
    clampedSize = FloatSize();

  if (clampedSize == m_size)
    return;

  m_size = clampedSize;

  m_layer->layer()->setBounds(flooredIntSize(m_size));
  // Note that we don't resize m_contentsLayer. It's up the caller to do that.
}

void GraphicsLayer::setTransform(const TransformationMatrix& transform) {
  m_transform = transform;
  platformLayer()->setTransform(
      TransformationMatrix::toSkMatrix44(m_transform));
}

void GraphicsLayer::setTransformOrigin(const FloatPoint3D& transformOrigin) {
  m_hasTransformOrigin = true;
  m_transformOrigin = transformOrigin;
  platformLayer()->setTransformOrigin(transformOrigin);
}

void GraphicsLayer::setShouldFlattenTransform(bool shouldFlatten) {
  if (shouldFlatten == m_shouldFlattenTransform)
    return;

  m_shouldFlattenTransform = shouldFlatten;

  m_layer->layer()->setShouldFlattenTransform(shouldFlatten);
}

void GraphicsLayer::setRenderingContext(int context) {
  if (m_renderingContext3d == context)
    return;

  m_renderingContext3d = context;
  m_layer->layer()->setRenderingContext(context);

  if (m_contentsLayer)
    m_contentsLayer->setRenderingContext(m_renderingContext3d);
}

bool GraphicsLayer::masksToBounds() const {
  return m_layer->layer()->masksToBounds();
}

void GraphicsLayer::setMasksToBounds(bool masksToBounds) {
  m_layer->layer()->setMasksToBounds(masksToBounds);
}

void GraphicsLayer::setDrawsContent(bool drawsContent) {
  // NOTE: This early-exit is only correct because we also properly call
  // WebLayer::setDrawsContent() whenever |m_contentsLayer| is set to a new
  // layer in setupContentsLayer().
  if (drawsContent == m_drawsContent)
    return;

  m_drawsContent = drawsContent;
  updateLayerIsDrawable();

  if (!drawsContent && m_paintController)
    m_paintController.reset();
}

void GraphicsLayer::setContentsVisible(bool contentsVisible) {
  // NOTE: This early-exit is only correct because we also properly call
  // WebLayer::setDrawsContent() whenever |m_contentsLayer| is set to a new
  // layer in setupContentsLayer().
  if (contentsVisible == m_contentsVisible)
    return;

  m_contentsVisible = contentsVisible;
  updateLayerIsDrawable();
}

void GraphicsLayer::setClipParent(WebLayer* parent) {
  m_hasClipParent = !!parent;
  m_layer->layer()->setClipParent(parent);
}

void GraphicsLayer::setScrollParent(WebLayer* parent) {
  m_hasScrollParent = !!parent;
  m_layer->layer()->setScrollParent(parent);
}

void GraphicsLayer::setBackgroundColor(const Color& color) {
  if (color == m_backgroundColor)
    return;

  m_backgroundColor = color;
  m_layer->layer()->setBackgroundColor(m_backgroundColor.rgb());
}

void GraphicsLayer::setContentsOpaque(bool opaque) {
  m_contentsOpaque = opaque;
  m_layer->layer()->setOpaque(m_contentsOpaque);
  clearContentsLayerIfUnregistered();
  if (m_contentsLayer)
    m_contentsLayer->setOpaque(opaque);
}

void GraphicsLayer::setMaskLayer(GraphicsLayer* maskLayer) {
  if (maskLayer == m_maskLayer)
    return;

  m_maskLayer = maskLayer;
  WebLayer* maskWebLayer = m_maskLayer ? m_maskLayer->platformLayer() : 0;
  m_layer->layer()->setMaskLayer(maskWebLayer);
}

void GraphicsLayer::setContentsClippingMaskLayer(
    GraphicsLayer* contentsClippingMaskLayer) {
  if (contentsClippingMaskLayer == m_contentsClippingMaskLayer)
    return;

  m_contentsClippingMaskLayer = contentsClippingMaskLayer;
  WebLayer* contentsLayer = contentsLayerIfRegistered();
  if (!contentsLayer)
    return;
  WebLayer* contentsClippingMaskWebLayer =
      m_contentsClippingMaskLayer ? m_contentsClippingMaskLayer->platformLayer()
                                  : 0;
  contentsLayer->setMaskLayer(contentsClippingMaskWebLayer);
  updateContentsRect();
}

void GraphicsLayer::setBackfaceVisibility(bool visible) {
  m_backfaceVisibility = visible;
  platformLayer()->setDoubleSided(m_backfaceVisibility);
}

void GraphicsLayer::setOpacity(float opacity) {
  float clampedOpacity = clampTo(opacity, 0.0f, 1.0f);
  m_opacity = clampedOpacity;
  platformLayer()->setOpacity(opacity);
}

void GraphicsLayer::setBlendMode(WebBlendMode blendMode) {
  if (m_blendMode == blendMode)
    return;
  m_blendMode = blendMode;
  platformLayer()->setBlendMode(blendMode);
}

void GraphicsLayer::setIsRootForIsolatedGroup(bool isolated) {
  if (m_isRootForIsolatedGroup == isolated)
    return;
  m_isRootForIsolatedGroup = isolated;
  platformLayer()->setIsRootForIsolatedGroup(isolated);
}

void GraphicsLayer::setContentsNeedsDisplay() {
  if (WebLayer* contentsLayer = contentsLayerIfRegistered()) {
    contentsLayer->invalidate();
    trackRasterInvalidation(*this, m_contentsRect, PaintInvalidationFull);
  }
}

void GraphicsLayer::setNeedsDisplay() {
  if (!drawsContent())
    return;

  // TODO(chrishtr): Stop invalidating the rects once
  // FrameView::paintRecursively() does so.
  m_layer->layer()->invalidate();
  for (size_t i = 0; i < m_linkHighlights.size(); ++i)
    m_linkHighlights[i]->invalidate();
  getPaintController().invalidateAll();

  trackRasterInvalidation(*this, IntRect(IntPoint(), expandedIntSize(m_size)),
                          PaintInvalidationFull);
}

void GraphicsLayer::setNeedsDisplayInRect(
    const IntRect& rect,
    PaintInvalidationReason invalidationReason,
    const DisplayItemClient& client) {
  if (!drawsContent())
    return;

  m_layer->layer()->invalidateRect(rect);
  if (firstPaintInvalidationTrackingEnabled())
    m_debugInfo.appendAnnotatedInvalidateRect(rect, invalidationReason);
  for (size_t i = 0; i < m_linkHighlights.size(); ++i)
    m_linkHighlights[i]->invalidate();

  trackRasterInvalidation(client, rect, invalidationReason);
}

void GraphicsLayer::setContentsRect(const IntRect& rect) {
  if (rect == m_contentsRect)
    return;

  m_contentsRect = rect;
  updateContentsRect();
}

void GraphicsLayer::setContentsToImage(
    Image* image,
    RespectImageOrientationEnum respectImageOrientation) {
  sk_sp<SkImage> skImage = image ? image->imageForCurrentFrame() : nullptr;

  if (image && skImage && image->isBitmapImage()) {
    if (respectImageOrientation == RespectImageOrientation) {
      ImageOrientation imageOrientation =
          toBitmapImage(image)->currentFrameOrientation();
      skImage =
          DragImage::resizeAndOrientImage(std::move(skImage), imageOrientation);
    }
  }

  if (image && skImage) {
    if (!m_imageLayer) {
      m_imageLayer =
          Platform::current()->compositorSupport()->createImageLayer();
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

WebLayer* GraphicsLayer::platformLayer() const {
  return m_layer->layer();
}

void GraphicsLayer::setFilters(CompositorFilterOperations filters) {
  platformLayer()->setFilters(filters.releaseCcFilterOperations());
}

void GraphicsLayer::setBackdropFilters(CompositorFilterOperations filters) {
  platformLayer()->setBackgroundFilters(filters.releaseCcFilterOperations());
}

void GraphicsLayer::setStickyPositionConstraint(
    const WebLayerStickyPositionConstraint& stickyConstraint) {
  m_layer->layer()->setStickyPositionConstraint(stickyConstraint);
}

void GraphicsLayer::setFilterQuality(SkFilterQuality filterQuality) {
  if (m_imageLayer)
    m_imageLayer->setNearestNeighbor(filterQuality == kNone_SkFilterQuality);
}

void GraphicsLayer::setPaintingPhase(GraphicsLayerPaintingPhase phase) {
  if (m_paintingPhase == phase)
    return;
  m_paintingPhase = phase;
  setNeedsDisplay();
}

void GraphicsLayer::addLinkHighlight(LinkHighlight* linkHighlight) {
  ASSERT(linkHighlight && !m_linkHighlights.contains(linkHighlight));
  m_linkHighlights.append(linkHighlight);
  linkHighlight->layer()->setLayerClient(this);
  updateChildList();
}

void GraphicsLayer::removeLinkHighlight(LinkHighlight* linkHighlight) {
  m_linkHighlights.remove(m_linkHighlights.find(linkHighlight));
  updateChildList();
}

void GraphicsLayer::setScrollableArea(ScrollableArea* scrollableArea,
                                      bool isVisualViewport) {
  if (m_scrollableArea == scrollableArea)
    return;

  m_scrollableArea = scrollableArea;

  // VisualViewport scrolling may involve pinch zoom and gets routed through
  // WebViewImpl explicitly rather than via GraphicsLayer::didScroll since it
  // needs to be set in tandem with the page scale delta.
  if (isVisualViewport)
    m_layer->layer()->setScrollClient(0);
  else
    m_layer->layer()->setScrollClient(this);
}

void GraphicsLayer::didScroll() {
  if (m_scrollableArea) {
    ScrollOffset newOffset =
        toFloatSize(m_layer->layer()->scrollPositionDouble() -
                    m_scrollableArea->scrollOrigin());

    // FrameView::setScrollOffset() doesn't work for compositor commits
    // (interacts poorly with programmatic scroll animations) so we need to use
    // the ScrollableArea version. The FrameView method should go away soon
    // anyway.
    m_scrollableArea->ScrollableArea::setScrollOffset(newOffset,
                                                      CompositorScroll);
  }
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
GraphicsLayer::TakeDebugInfo(cc::Layer* layer) {
  std::unique_ptr<base::trace_event::TracedValue> tracedValue(
      m_debugInfo.asTracedValue());
  tracedValue->SetString(
      "layer_name", WTF::StringUTF8Adaptor(debugName(layer)).asStringPiece());
  return std::move(tracedValue);
}

void GraphicsLayer::didUpdateMainThreadScrollingReasons() {
  m_debugInfo.setMainThreadScrollingReasons(
      platformLayer()->mainThreadScrollingReasons());
}

void GraphicsLayer::didChangeScrollbarsHidden(bool hidden) {
  if (m_scrollableArea)
    m_scrollableArea->setScrollbarsHidden(hidden);
}

PaintController& GraphicsLayer::getPaintController() {
  RELEASE_ASSERT(drawsContent());
  if (!m_paintController)
    m_paintController = PaintController::create();
  return *m_paintController;
}

void GraphicsLayer::setElementId(const CompositorElementId& id) {
  if (WebLayer* layer = platformLayer())
    layer->setElementId(id);
}

CompositorElementId GraphicsLayer::elementId() const {
  if (WebLayer* layer = platformLayer())
    return layer->elementId();
  return CompositorElementId();
}

void GraphicsLayer::setCompositorMutableProperties(uint32_t properties) {
  if (WebLayer* layer = platformLayer())
    layer->setCompositorMutableProperties(properties);
}

sk_sp<SkPicture> GraphicsLayer::capturePicture() {
  if (!drawsContent())
    return nullptr;

  IntSize intSize = expandedIntSize(size());
  GraphicsContext graphicsContext(getPaintController());
  graphicsContext.beginRecording(IntRect(IntPoint(0, 0), intSize));
  getPaintController().paintArtifact().replay(graphicsContext);
  return graphicsContext.endRecording();
}

static bool pixelComponentsDiffer(int c1, int c2) {
  // Compare strictly for saturated values.
  if (c1 == 0 || c1 == 255 || c2 == 0 || c2 == 255)
    return c1 != c2;
  // Tolerate invisible differences that may occur in gradients etc.
  return abs(c1 - c2) > 2;
}

static bool pixelsDiffer(SkColor p1, SkColor p2) {
  return pixelComponentsDiffer(SkColorGetA(p1), SkColorGetA(p2)) ||
         pixelComponentsDiffer(SkColorGetR(p1), SkColorGetR(p2)) ||
         pixelComponentsDiffer(SkColorGetG(p1), SkColorGetG(p2)) ||
         pixelComponentsDiffer(SkColorGetB(p1), SkColorGetB(p2));
}

void GraphicsLayer::checkPaintUnderInvalidations(const SkPicture& newPicture) {
  if (!drawsContent())
    return;

  RasterInvalidationTracking* tracking =
      rasterInvalidationTrackingMap().find(this);
  if (!tracking)
    return;

  if (!tracking->lastPaintedPicture)
    return;

  IntRect rect = intersection(tracking->lastInterestRect, interestRect());
  if (rect.isEmpty())
    return;

  SkBitmap oldBitmap;
  oldBitmap.allocPixels(
      SkImageInfo::MakeN32Premul(rect.width(), rect.height()));
  {
    SkCanvas canvas(oldBitmap);
    canvas.clear(SK_ColorTRANSPARENT);
    canvas.translate(-rect.x(), -rect.y());
    canvas.drawPicture(tracking->lastPaintedPicture.get());
  }

  SkBitmap newBitmap;
  newBitmap.allocPixels(
      SkImageInfo::MakeN32Premul(rect.width(), rect.height()));
  {
    SkCanvas canvas(newBitmap);
    canvas.clear(SK_ColorTRANSPARENT);
    canvas.translate(-rect.x(), -rect.y());
    canvas.drawPicture(&newPicture);
  }

  oldBitmap.lockPixels();
  newBitmap.lockPixels();
  int mismatchingPixels = 0;
  static const int maxMismatchesToReport = 50;
  for (int bitmapY = 0; bitmapY < rect.height(); ++bitmapY) {
    int layerY = bitmapY + rect.y();
    for (int bitmapX = 0; bitmapX < rect.width(); ++bitmapX) {
      int layerX = bitmapX + rect.x();
      SkColor oldPixel = oldBitmap.getColor(bitmapX, bitmapY);
      SkColor newPixel = newBitmap.getColor(bitmapX, bitmapY);
      if (pixelsDiffer(oldPixel, newPixel) &&
          !tracking->rasterInvalidationRegionSinceLastPaint.contains(
              IntPoint(layerX, layerY))) {
        if (mismatchingPixels < maxMismatchesToReport) {
          UnderPaintInvalidation underPaintInvalidation = {layerX, layerY,
                                                           oldPixel, newPixel};
          tracking->underPaintInvalidations.append(underPaintInvalidation);
          LOG(ERROR) << debugName()
                     << " Uninvalidated old/new pixels mismatch at " << layerX
                     << "," << layerY << " old:" << std::hex << oldPixel
                     << " new:" << newPixel;
        } else if (mismatchingPixels == maxMismatchesToReport) {
          LOG(ERROR) << "and more...";
        }
        ++mismatchingPixels;
        *newBitmap.getAddr32(bitmapX, bitmapY) =
            SkColorSetARGB(0xFF, 0xA0, 0, 0);  // Dark red.
      } else {
        *newBitmap.getAddr32(bitmapX, bitmapY) = SK_ColorTRANSPARENT;
      }
    }
  }
  oldBitmap.unlockPixels();
  newBitmap.unlockPixels();

  // Visualize under-invalidations by overlaying the new bitmap (containing red
  // pixels indicating under-invalidations, and transparent pixels otherwise)
  // onto the painting.
  SkPictureRecorder recorder;
  recorder.beginRecording(rect);
  recorder.getRecordingCanvas()->drawBitmap(newBitmap, rect.x(), rect.y());
  sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();
  getPaintController().appendDebugDrawingAfterCommit(
      *this, picture, offsetFromLayoutObjectWithSubpixelAccumulation());
}

}  // namespace blink

#ifndef NDEBUG
void showGraphicsLayerTree(const blink::GraphicsLayer* layer) {
  if (!layer) {
    LOG(INFO) << "Cannot showGraphicsLayerTree for (nil).";
    return;
  }

  String output = layer->layerTreeAsText(blink::LayerTreeIncludesDebugInfo);
  LOG(INFO) << output.utf8().data();
}
#endif
