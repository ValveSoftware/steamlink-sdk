// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/Transform3DDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/transforms/AffineTransform.h"
#include "public/platform/WebDisplayItemList.h"

namespace blink {

void BeginTransform3DDisplayItem::replay(GraphicsContext& context) const {
  TransformationMatrix transform(m_transform);
  transform.applyTransformOrigin(m_transformOrigin);
  context.save();
  context.concatCTM(transform.toAffineTransform());
}

void BeginTransform3DDisplayItem::appendToWebDisplayItemList(
    const IntRect& visualRect,
    WebDisplayItemList* list) const {
  // TODO(jbroman): The compositor will need the transform origin separately.
  TransformationMatrix transform(m_transform);
  transform.applyTransformOrigin(m_transformOrigin);
  list->appendTransformItem(TransformationMatrix::toSkMatrix44(transform));
}

#ifndef NDEBUG
void BeginTransform3DDisplayItem::dumpPropertiesAsDebugString(
    StringBuilder& stringBuilder) const {
  PairedBeginDisplayItem::dumpPropertiesAsDebugString(stringBuilder);
  TransformationMatrix::DecomposedType decomposition;
  if (m_transform.decompose(decomposition)) {
    stringBuilder.append(String::format(
        ", translate: [%lf,%lf,%lf], scale: [%lf,%lf,%lf], skew: "
        "[%lf,%lf,%lf], quaternion: [%lf,%lf,%lf,%lf], perspective: "
        "[%lf,%lf,%lf,%lf]",
        decomposition.translateX, decomposition.translateY,
        decomposition.translateZ, decomposition.scaleX, decomposition.scaleY,
        decomposition.scaleZ, decomposition.skewXY, decomposition.skewXZ,
        decomposition.skewYZ, decomposition.quaternionX,
        decomposition.quaternionY, decomposition.quaternionZ,
        decomposition.quaternionW, decomposition.perspectiveX,
        decomposition.perspectiveY, decomposition.perspectiveZ,
        decomposition.perspectiveW));
  }
  stringBuilder.append(
      String::format(", transformOrigin: [%f,%f,%f]", m_transformOrigin.x(),
                     m_transformOrigin.y(), m_transformOrigin.z()));
}
#endif

void EndTransform3DDisplayItem::replay(GraphicsContext& context) const {
  context.restore();
}

void EndTransform3DDisplayItem::appendToWebDisplayItemList(
    const IntRect& visualRect,
    WebDisplayItemList* list) const {
  list->appendEndTransformItem();
}

}  // namespace blink
