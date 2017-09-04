// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ClipPathClipper.h"

#include "core/layout/svg/LayoutSVGResourceClipper.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/SVGClipPainter.h"
#include "core/style/ClipPathOperation.h"
#include "platform/graphics/paint/ClipPathRecorder.h"

namespace blink {

namespace {

LayoutSVGResourceClipper* resolveElementReference(
    const LayoutObject& layoutObject,
    const ReferenceClipPathOperation& referenceClipPathOperation) {
  if (layoutObject.isSVG() && !layoutObject.isSVGRoot()) {
    // The reference will have been resolved in
    // SVGResources::buildResources, so we can just use the LayoutObject's
    // SVGResources.
    SVGResources* resources =
        SVGResourcesCache::cachedResourcesForLayoutObject(&layoutObject);
    return resources ? resources->clipper() : nullptr;
  }
  // TODO(fs): Doesn't work with external SVG references (crbug.com/109212.)
  Node* targetNode = layoutObject.node();
  if (!targetNode)
    return nullptr;
  SVGElement* element =
      referenceClipPathOperation.findElement(targetNode->treeScope());
  if (!isSVGClipPathElement(element) || !element->layoutObject())
    return nullptr;
  return toLayoutSVGResourceClipper(
      toLayoutSVGResourceContainer(element->layoutObject()));
}

}  // namespace

ClipPathClipper::ClipPathClipper(GraphicsContext& context,
                                 ClipPathOperation& clipPathOperation,
                                 const LayoutObject& layoutObject,
                                 const FloatRect& referenceBox,
                                 const FloatPoint& origin)
    : m_resourceClipper(nullptr),
      m_clipperState(ClipperState::NotApplied),
      m_layoutObject(layoutObject),
      m_context(context) {
  if (clipPathOperation.type() == ClipPathOperation::SHAPE) {
    ShapeClipPathOperation& shape = toShapeClipPathOperation(clipPathOperation);
    if (!shape.isValid())
      return;
    m_clipPathRecorder.emplace(context, layoutObject, shape.path(referenceBox));
    m_clipperState = ClipperState::AppliedPath;
  } else {
    DCHECK_EQ(clipPathOperation.type(), ClipPathOperation::REFERENCE);
    LayoutSVGResourceClipper* clipper = resolveElementReference(
        layoutObject, toReferenceClipPathOperation(clipPathOperation));
    if (!clipper)
      return;
    // Compute the (conservative) bounds of the clip-path.
    FloatRect clipPathBounds = clipper->resourceBoundingBox(referenceBox);
    // When SVG applies the clip, and the coordinate system is "userspace on
    // use", we must explicitly pass in the offset to have the clip paint in the
    // correct location. When the coordinate system is "object bounding box" the
    // offset should already be accounted for in the reference box.
    FloatPoint originTranslation;
    if (clipper->clipPathUnits() == SVGUnitTypes::kSvgUnitTypeUserspaceonuse) {
      clipPathBounds.moveBy(origin);
      originTranslation = origin;
    }
    if (!SVGClipPainter(*clipper).prepareEffect(
            layoutObject, referenceBox, clipPathBounds, originTranslation,
            context, m_clipperState))
      return;
    m_resourceClipper = clipper;
  }
}

ClipPathClipper::~ClipPathClipper() {
  if (m_resourceClipper)
    SVGClipPainter(*m_resourceClipper)
        .finishEffect(m_layoutObject, m_context, m_clipperState);
}

}  // namespace blink
