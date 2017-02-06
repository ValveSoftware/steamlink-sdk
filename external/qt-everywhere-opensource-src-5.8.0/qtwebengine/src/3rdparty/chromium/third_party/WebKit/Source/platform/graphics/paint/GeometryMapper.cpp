// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/GeometryMapper.h"

#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"

namespace blink {

FloatRect GeometryMapper::LocalToVisualRectInAncestorSpace(
    const FloatRect& rect,
    const PropertyTreeState& localState,
    const PropertyTreeState& ancestorState)
{
    const auto transformMatrix = LocalToAncestorMatrix(localState.transform, ancestorState);
    FloatRect mappedRect = transformMatrix.mapRect(rect);

    const auto clipRect = LocalToAncestorClipRect(localState, ancestorState);

    mappedRect.intersect(clipRect);
    return mappedRect;
}

FloatRect GeometryMapper::LocalToAncestorRect(
    const FloatRect& rect,
    const PropertyTreeState& localState,
    const PropertyTreeState& ancestorState)
{
    const auto transformMatrix = LocalToAncestorMatrix(localState.transform, ancestorState);
    return transformMatrix.mapRect(rect);
}

FloatRect GeometryMapper::AncestorToLocalRect(
    const FloatRect& rect,
    const PropertyTreeState& localState,
    const PropertyTreeState& ancestorState, bool* success)
{
    const auto& transformMatrix = LocalToAncestorMatrix(localState.transform, ancestorState);
    if (!transformMatrix.isInvertible()) {
        *success = false;
        return FloatRect();
    }
    *success = true;

    // TODO(chrishtr): Cache the inverse?
    return transformMatrix.inverse().mapRect(rect);
}

PrecomputedDataForAncestor& GeometryMapper::GetPrecomputedDataForAncestor(const PropertyTreeState& ancestorState)
{
    auto addResult = m_data.add(ancestorState.transform, nullptr);
    if (addResult.isNewEntry)
        addResult.storedValue->value = PrecomputedDataForAncestor::create();
    return *addResult.storedValue->value;
}

const FloatRect& GeometryMapper::LocalToAncestorClipRect(
    const PropertyTreeState& localState,
    const PropertyTreeState& ancestorState)
{
    PrecomputedDataForAncestor& precomputedData = GetPrecomputedDataForAncestor(ancestorState);
    const ClipPaintPropertyNode* clipNode = localState.clip;
    Vector<const ClipPaintPropertyNode*> intermediateNodes;
    FloatRect clip(LayoutRect::infiniteIntRect());

    bool found = false;
    // Iterate over the path from localState.clip to ancestorState.clip. Stop if we've found a memoized (precomputed) clip
    // for any particular node.
    while (clipNode) {
        auto it = precomputedData.toAncestorClipRects.find(clipNode);
        if (it != precomputedData.toAncestorClipRects.end()) {
            clip = it->value;
            found = true;
            break;
        }
        intermediateNodes.append(clipNode);

        if (clipNode == ancestorState.clip)
            break;

        clipNode = clipNode->parent();
    }
    // It's illegal to ask for a local-to-ancestor clip when the ancestor is not an ancestor.
    DCHECK(clipNode == ancestorState.clip || found);

    // Iterate down from the top intermediate node found in the previous loop, computing and memoizing clip rects as we go.
    for (auto it = intermediateNodes.rbegin(); it != intermediateNodes.rend(); ++it) {
        if ((*it) != ancestorState.clip) {
            const TransformationMatrix transformMatrix = LocalToAncestorMatrix((*it)->localTransformSpace(), ancestorState);
            FloatRect mappedRect = transformMatrix.mapRect((*it)->clipRect().rect());
            clip.intersect(mappedRect);
        }

        precomputedData.toAncestorClipRects.set(*it, clip);
    }

    return precomputedData.toAncestorClipRects.find(localState.clip)->value;
}

const TransformationMatrix& GeometryMapper::LocalToAncestorMatrix(
    const TransformPaintPropertyNode* localTransformNode,
    const PropertyTreeState& ancestorState) {
    PrecomputedDataForAncestor& precomputedData = GetPrecomputedDataForAncestor(ancestorState);

    const TransformPaintPropertyNode* transformNode = localTransformNode;
    Vector<const TransformPaintPropertyNode*> intermediateNodes;
    TransformationMatrix transformMatrix;

    bool found = false;
    // Iterate over the path from localTransformNode to ancestorState.transform. Stop if we've found a memoized (precomputed) transform
    // for any particular node.
    while (transformNode) {
        auto it = precomputedData.toAncestorTransforms.find(transformNode);
        if (it != precomputedData.toAncestorTransforms.end()) {
            transformMatrix = it->value;
            found = true;
            break;
        }

        intermediateNodes.append(transformNode);

        if (transformNode == ancestorState.transform)
            break;

        transformNode = transformNode->parent();
    }
    // It's illegal to ask for a local-to-ancestor matrix when the ancestor is not an ancestor.
    DCHECK(transformNode == ancestorState.transform || found);

    // Iterate down from the top intermediate node found in the previous loop, computing and memoizing transforms as we go.
    for (auto it = intermediateNodes.rbegin(); it != intermediateNodes.rend(); it++) {
        if ((*it) != ancestorState.transform) {
            TransformationMatrix localTransformMatrix = (*it)->matrix();
            localTransformMatrix.applyTransformOrigin((*it)->origin());
            transformMatrix = localTransformMatrix * transformMatrix;
        }

        precomputedData.toAncestorTransforms.set(*it, transformMatrix);
    }
    return precomputedData.toAncestorTransforms.find(localTransformNode)->value;
}

} // namespace blink
