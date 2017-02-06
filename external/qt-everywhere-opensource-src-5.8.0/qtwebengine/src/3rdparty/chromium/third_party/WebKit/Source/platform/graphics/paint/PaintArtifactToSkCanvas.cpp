// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintArtifactToSkCanvas.h"

#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/graphics/paint/PaintChunk.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/transforms/AffineTransform.h"
#include "platform/transforms/TransformationMatrix.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"

namespace blink {

namespace {

void paintDisplayItemToSkCanvas(const DisplayItem& displayItem, SkCanvas* canvas)
{
    DisplayItem::Type type = displayItem.getType();

    if (DisplayItem::isDrawingType(type)) {
        canvas->drawPicture(static_cast<const DrawingDisplayItem&>(displayItem).picture());
        return;
    }
}

} // namespace

// Compute the list of nodes from 'a' to commonAncestor(a, b) and 'b' to
// commonAncestor(a, b), exclusive.
//
// For the following tree:
//   _root_
//  |      |
//  a      d
//  |      |
//  b      e
//  |
//  c
// The common ancestor of 'c' and 'e' is 'root'. The paths would be:
// path from c to commonAncestor(c,e) = [b,a]
// path from e to commonAncestor(c,e) = [d]
static void computePathsToCommonAncestor(const EffectPaintPropertyNode* a, const EffectPaintPropertyNode* b, Vector<const EffectPaintPropertyNode*>& aToCommonAncestor, Vector<const EffectPaintPropertyNode*>& bToCommonAncestor)
{
    // Calculate the path from a -> root.
    const EffectPaintPropertyNode* current = a;
    while (current) {
        aToCommonAncestor.append(current);
        current = current->parent();
    }

    // Calculate the path from b -> root.
    current = b;
    while (current) {
        bToCommonAncestor.append(current);
        current = current->parent();
    }

    // Pop nodes from root -> a and root -> b while the last node is equal so we
    // are left with just the common ancestor path, not including the common
    // ancestor itself.
    while (aToCommonAncestor.size() && bToCommonAncestor.size() && aToCommonAncestor.last() == bToCommonAncestor.last()) {
        aToCommonAncestor.removeLast();
        bToCommonAncestor.removeLast();
    }
}

static void applyEffectNodesToCanvas(const EffectPaintPropertyNode* previousEffect, const EffectPaintPropertyNode* currentEffect, SkCanvas* canvas)
{
    if (previousEffect == currentEffect)
        return;

    Vector<const EffectPaintPropertyNode*> effectsToUnapply;
    Vector<const EffectPaintPropertyNode*> effectsToApply;
    computePathsToCommonAncestor(previousEffect, currentEffect, effectsToUnapply, effectsToApply);

    size_t popEffectCount = effectsToUnapply.size();
    for (size_t popEffect = 0; popEffect < popEffectCount; popEffect++)
        canvas->restore();

    for (Vector<const EffectPaintPropertyNode*>::reverse_iterator it = effectsToApply.rbegin(); it != effectsToApply.rend(); ++it) {
        const EffectPaintPropertyNode* node = *it;
        ASSERT(node);
        SkPaint layerPaint;
        layerPaint.setAlpha(static_cast<unsigned char>(node->opacity() * 255));
        canvas->saveLayer(nullptr, &layerPaint);
    }
}

static TransformationMatrix totalTransform(const TransformPaintPropertyNode* currentSpace)
{
    TransformationMatrix matrix;
    for (; currentSpace; currentSpace = currentSpace->parent()) {
        TransformationMatrix localMatrix = currentSpace->matrix();
        localMatrix.applyTransformOrigin(currentSpace->origin());
        matrix = localMatrix * matrix;
    }
    return matrix;
}

void paintArtifactToSkCanvas(const PaintArtifact& artifact, SkCanvas* canvas)
{
    SkAutoCanvasRestore restore(canvas, true);
    const DisplayItemList& displayItems = artifact.getDisplayItemList();
    const EffectPaintPropertyNode* previousEffect = nullptr;
    for (const PaintChunk& chunk : artifact.paintChunks()) {
        // Setup the canvas clip state first because it clobbers matrix state.
        for (const ClipPaintPropertyNode* currentClipNode = chunk.properties.clip.get();
            currentClipNode; currentClipNode = currentClipNode->parent()) {
            canvas->setMatrix(TransformationMatrix::toSkMatrix44(totalTransform(currentClipNode->localTransformSpace())));
            canvas->clipRRect(currentClipNode->clipRect());
        }

        // Set the canvas state to match the paint properties.
        TransformationMatrix combinedMatrix = totalTransform(chunk.properties.transform.get());
        canvas->setMatrix(TransformationMatrix::toSkMatrix44(combinedMatrix));

        // Push and pop layers on the SkCanvas as necessary to implement the
        // current effect.
        // TODO(pdr): This will need to be revisited for non-opacity effects
        // such as filters which require interleaving with transforms.
        const EffectPaintPropertyNode* chunkEffect = chunk.properties.effect.get();
        applyEffectNodesToCanvas(previousEffect, chunkEffect, canvas);
        previousEffect = chunkEffect;

        // Draw the display items in the paint chunk.
        for (const auto& displayItem : displayItems.itemsInPaintChunk(chunk))
            paintDisplayItemToSkCanvas(displayItem, canvas);
    }
}

sk_sp<const SkPicture> paintArtifactToSkPicture(const PaintArtifact& artifact, const SkRect& bounds)
{
    SkPictureRecorder recorder;
    SkCanvas* canvas = recorder.beginRecording(bounds);
    paintArtifactToSkCanvas(artifact, canvas);
    return recorder.finishRecordingAsPicture();
}

} // namespace blink
