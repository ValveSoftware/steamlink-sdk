// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintPropertyTreeBuilder.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintLayer.h"
#include "platform/transforms/TransformationMatrix.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

void PaintPropertyTreeBuilder::buildTreeRootNodes(FrameView& rootFrame, PaintPropertyTreeBuilderContext& context)
{
    // Only create extra root clip and transform nodes when RLS is enabled, because the main frame
    // unconditionally create frame translation / clip nodes otherwise.
    if (rootFrame.frame().settings() && rootFrame.frame().settings()->rootLayerScrolls()) {
        transformRoot = TransformPaintPropertyNode::create(TransformationMatrix(), FloatPoint3D(), nullptr);
        context.currentTransform = context.transformForAbsolutePosition = context.transformForFixedPosition = transformRoot.get();

        clipRoot = ClipPaintPropertyNode::create(transformRoot, FloatRoundedRect(LayoutRect::infiniteIntRect()), nullptr);
        context.currentClip = context.clipForAbsolutePosition = context.clipForFixedPosition = clipRoot.get();
    }

    // The root frame never creates effect node so we unconditionally create a root node here.
    effectRoot = EffectPaintPropertyNode::create(1.0, nullptr);
    context.currentEffect = effectRoot.get();
}

void PaintPropertyTreeBuilder::buildTreeNodes(FrameView& frameView, PaintPropertyTreeBuilderContext& context)
{
    // TODO(pdr): Creating paint properties for FrameView here will not be
    // needed once settings()->rootLayerScrolls() is enabled.
    // TODO(pdr): Make this conditional on the rootLayerScrolls setting.

    TransformationMatrix frameTranslate;
    frameTranslate.translate(frameView.x() + context.paintOffset.x(), frameView.y() + context.paintOffset.y());
    RefPtr<TransformPaintPropertyNode> newTransformNodeForPreTranslation = TransformPaintPropertyNode::create(frameTranslate, FloatPoint3D(), context.currentTransform);
    context.transformForFixedPosition = newTransformNodeForPreTranslation.get();
    context.paintOffsetForFixedPosition = LayoutPoint();

    FloatRoundedRect contentClip(IntRect(IntPoint(), frameView.visibleContentSize()));
    RefPtr<ClipPaintPropertyNode> newClipNodeForContentClip = ClipPaintPropertyNode::create(newTransformNodeForPreTranslation.get(), contentClip, context.currentClip);
    context.currentClip = context.clipForAbsolutePosition = context.clipForFixedPosition = newClipNodeForContentClip.get();

    DoubleSize scrollOffset = frameView.scrollOffsetDouble();
    TransformationMatrix frameScroll;
    frameScroll.translate(-scrollOffset.width(), -scrollOffset.height());
    RefPtr<TransformPaintPropertyNode> newTransformNodeForScrollTranslation = TransformPaintPropertyNode::create(frameScroll, FloatPoint3D(), newTransformNodeForPreTranslation);
    context.currentTransform = context.transformForAbsolutePosition = newTransformNodeForScrollTranslation.get();
    context.paintOffset = LayoutPoint();
    context.paintOffsetForAbsolutePosition = LayoutPoint();
    context.containerForAbsolutePosition = nullptr;

    frameView.setPreTranslation(newTransformNodeForPreTranslation.release());
    frameView.setScrollTranslation(newTransformNodeForScrollTranslation.release());
    frameView.setContentClip(newClipNodeForContentClip.release());
}

void PaintPropertyTreeBuilder::updatePaintOffsetTranslation(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (object.isBoxModelObject()) {
        // TODO(trchen): Eliminate PaintLayer dependency.
        PaintLayer* layer = toLayoutBoxModelObject(object).layer();
        if (!layer || !layer->paintsWithTransform(GlobalPaintNormalPhase))
            return;
    }

    if (context.paintOffset == LayoutPoint())
        return;

    // We should use the same subpixel paint offset values for snapping regardless of whether a
    // transform is present. If there is a transform we round the paint offset but keep around
    // the residual fractional component for the transformed content to paint with.
    // In spv1 this was called "subpixel accumulation". For more information, see
    // PaintLayer::subpixelAccumulation() and PaintLayerPainter::paintFragmentByApplyingTransform.
    IntPoint roundedPaintOffset = roundedIntPoint(context.paintOffset);
    LayoutPoint fractionalPaintOffset = LayoutPoint(context.paintOffset - roundedPaintOffset);

    RefPtr<TransformPaintPropertyNode> paintOffsetTranslation = TransformPaintPropertyNode::create(
        TransformationMatrix().translate(roundedPaintOffset.x(), roundedPaintOffset.y()),
        FloatPoint3D(), context.currentTransform);
    context.currentTransform = paintOffsetTranslation.get();
    context.paintOffset = fractionalPaintOffset;
    object.getMutableForPainting().ensureObjectPaintProperties().setPaintOffsetTranslation(paintOffsetTranslation.release());
}

static FloatPoint3D transformOrigin(const LayoutBox& box)
{
    const ComputedStyle& style = box.styleRef();
    FloatSize borderBoxSize(box.size());
    return FloatPoint3D(
        floatValueForLength(style.transformOriginX(), borderBoxSize.width()),
        floatValueForLength(style.transformOriginY(), borderBoxSize.height()),
        style.transformOriginZ());
}

void PaintPropertyTreeBuilder::updateTransform(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (object.isSVG() && !object.isSVGRoot()) {
        // SVG does not use paint offset internally.
        DCHECK(context.paintOffset == LayoutPoint());

        // FIXME(pdr): Check for the presence of a transform instead of the value. Checking for an
        // identity matrix will cause the property tree structure to change during animations if
        // the animation passes through the identity matrix.
        // FIXME(pdr): Refactor this so all non-root SVG objects use the same transform function.
        const AffineTransform& transform = object.isSVGForeignObject() ? object.localSVGTransform() : object.localToSVGParentTransform();
        if (transform.isIdentity())
            return;

        // The origin is included in the local transform, so use an empty origin.
        RefPtr<TransformPaintPropertyNode> svgTransform = TransformPaintPropertyNode::create(
            transform, FloatPoint3D(0, 0, 0), context.currentTransform);
        context.currentTransform = svgTransform.get();
        object.getMutableForPainting().ensureObjectPaintProperties().setTransform(svgTransform.release());
        return;
    }

    const ComputedStyle& style = object.styleRef();
    if (!object.isBox() || !style.hasTransform())
        return;

    TransformationMatrix matrix;
    style.applyTransform(matrix, toLayoutBox(object).size(), ComputedStyle::ExcludeTransformOrigin,
        ComputedStyle::IncludeMotionPath, ComputedStyle::IncludeIndependentTransformProperties);
    RefPtr<TransformPaintPropertyNode> transformNode = TransformPaintPropertyNode::create(
        matrix, transformOrigin(toLayoutBox(object)), context.currentTransform);
    context.currentTransform = transformNode.get();
    object.getMutableForPainting().ensureObjectPaintProperties().setTransform(transformNode.release());
}

void PaintPropertyTreeBuilder::updateEffect(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (!object.styleRef().hasOpacity())
        return;
    RefPtr<EffectPaintPropertyNode> effectNode = EffectPaintPropertyNode::create(object.styleRef().opacity(), context.currentEffect);
    context.currentEffect = effectNode.get();
    object.getMutableForPainting().ensureObjectPaintProperties().setEffect(effectNode.release());
}

void PaintPropertyTreeBuilder::updateCssClip(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (!object.hasClip())
        return;
    ASSERT(object.canContainAbsolutePositionObjects());

    // Create clip node for descendants that are not fixed position.
    // We don't have to setup context.clipForAbsolutePosition here because this object must be
    // a container for absolute position descendants, and will copy from in-flow context later
    // at updateOutOfFlowContext() step.
    LayoutRect clipRect = toLayoutBox(object).clipRect(context.paintOffset);
    RefPtr<ClipPaintPropertyNode> clipNode = ClipPaintPropertyNode::create(
        context.currentTransform,
        FloatRoundedRect(FloatRect(clipRect)),
        context.currentClip);
    context.currentClip = clipNode.get();
    object.getMutableForPainting().ensureObjectPaintProperties().setCssClip(clipNode.release());
}

void PaintPropertyTreeBuilder::updateLocalBorderBoxContext(const LayoutObject& object, const PaintPropertyTreeBuilderContext& context)
{
    // Note: Currently only layer painter makes use of the pre-computed context.
    // This condition may be loosened with no adverse effects beside memory use.
    if (!object.hasLayer())
        return;

    std::unique_ptr<ObjectPaintProperties::LocalBorderBoxProperties> borderBoxContext =
        wrapUnique(new ObjectPaintProperties::LocalBorderBoxProperties);
    borderBoxContext->paintOffset = context.paintOffset;
    borderBoxContext->transform = context.currentTransform;
    borderBoxContext->clip = context.currentClip;
    borderBoxContext->effect = context.currentEffect;
    object.getMutableForPainting().ensureObjectPaintProperties().setLocalBorderBoxProperties(std::move(borderBoxContext));
}

// TODO(trchen): Remove this once we bake the paint offset into frameRect.
void PaintPropertyTreeBuilder::updateScrollbarPaintOffset(const LayoutObject& object, const PaintPropertyTreeBuilderContext& context)
{
    IntPoint roundedPaintOffset = roundedIntPoint(context.paintOffset);
    if (roundedPaintOffset == IntPoint())
        return;

    if (!object.isBoxModelObject())
        return;
    PaintLayerScrollableArea* scrollableArea = toLayoutBoxModelObject(object).getScrollableArea();
    if (!scrollableArea)
        return;
    if (!scrollableArea->horizontalScrollbar() && !scrollableArea->verticalScrollbar())
        return;

    auto paintOffset = TransformationMatrix().translate(roundedPaintOffset.x(), roundedPaintOffset.y());
    object.getMutableForPainting().ensureObjectPaintProperties().setScrollbarPaintOffset(
        TransformPaintPropertyNode::create(paintOffset, FloatPoint3D(), context.currentTransform));
}

void PaintPropertyTreeBuilder::updateOverflowClip(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (!object.isBox())
        return;
    const LayoutBox& box = toLayoutBox(object);

    // The <input> elements can't have contents thus CSS overflow property doesn't apply.
    // However for layout purposes we do generate child layout objects for them, e.g. button label.
    // We should clip the overflow from those children. This is called control clip and we
    // technically treat them like overflow clip.
    LayoutRect clipRect;
    if (box.hasControlClip())
        clipRect = box.controlClipRect(context.paintOffset);
    else if (box.hasOverflowClip())
        clipRect = box.overflowClipRect(context.paintOffset);
    else
        return;

    RefPtr<ClipPaintPropertyNode> borderRadiusClip;
    if (box.styleRef().hasBorderRadius()) {
        auto innerBorder = box.styleRef().getRoundedInnerBorderFor(
            LayoutRect(context.paintOffset, box.size()));
        borderRadiusClip = ClipPaintPropertyNode::create(
            context.currentTransform, innerBorder, context.currentClip);
    }

    RefPtr<ClipPaintPropertyNode> overflowClip = ClipPaintPropertyNode::create(
        context.currentTransform,
        FloatRoundedRect(FloatRect(clipRect)),
        borderRadiusClip ? borderRadiusClip.release() : context.currentClip);
    context.currentClip = overflowClip.get();
    object.getMutableForPainting().ensureObjectPaintProperties().setOverflowClip(overflowClip.release());
}

static FloatPoint perspectiveOrigin(const LayoutBox& box)
{
    const ComputedStyle& style = box.styleRef();
    FloatSize borderBoxSize(box.size());
    return FloatPoint(
        floatValueForLength(style.perspectiveOriginX(), borderBoxSize.width()),
        floatValueForLength(style.perspectiveOriginY(), borderBoxSize.height()));
}

void PaintPropertyTreeBuilder::updatePerspective(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    const ComputedStyle& style = object.styleRef();
    if (!object.isBox() || !style.hasPerspective())
        return;

    RefPtr<TransformPaintPropertyNode> perspective = TransformPaintPropertyNode::create(
        TransformationMatrix().applyPerspective(style.perspective()),
        perspectiveOrigin(toLayoutBox(object)) + toLayoutSize(context.paintOffset),
        context.currentTransform);
    context.currentTransform = perspective.get();
    object.getMutableForPainting().ensureObjectPaintProperties().setPerspective(perspective.release());
}

void PaintPropertyTreeBuilder::updateSvgLocalToBorderBoxTransform(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (!object.isSVGRoot())
        return;

    AffineTransform transform = AffineTransform::translation(context.paintOffset.x().toFloat(), context.paintOffset.y().toFloat());
    transform *= toLayoutSVGRoot(object).localToBorderBoxTransform();
    if (transform.isIdentity())
        return;

    RefPtr<TransformPaintPropertyNode> svgLocalToBorderBoxTransform = TransformPaintPropertyNode::create(
        transform, FloatPoint3D(0, 0, 0), context.currentTransform);
    context.currentTransform = svgLocalToBorderBoxTransform.get();
    context.paintOffset = LayoutPoint();
    object.getMutableForPainting().ensureObjectPaintProperties().setSvgLocalToBorderBoxTransform(svgLocalToBorderBoxTransform.release());
}

void PaintPropertyTreeBuilder::updateScrollTranslation(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (!object.isBoxModelObject() || !object.hasOverflowClip())
        return;

    PaintLayer* layer = toLayoutBoxModelObject(object).layer();
    ASSERT(layer);
    DoubleSize scrollOffset = layer->getScrollableArea()->scrollOffset();
    if (scrollOffset.isZero() && !layer->scrollsOverflow())
        return;

    RefPtr<TransformPaintPropertyNode> scrollTranslation = TransformPaintPropertyNode::create(
        TransformationMatrix().translate(-scrollOffset.width(), -scrollOffset.height()),
        FloatPoint3D(),
        context.currentTransform);
    context.currentTransform = scrollTranslation.get();
    object.getMutableForPainting().ensureObjectPaintProperties().setScrollTranslation(scrollTranslation.release());
}

void PaintPropertyTreeBuilder::updateOutOfFlowContext(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (object.canContainAbsolutePositionObjects()) {
        context.transformForAbsolutePosition = context.currentTransform;
        context.paintOffsetForAbsolutePosition = context.paintOffset;
        context.clipForAbsolutePosition = context.currentClip;
        context.containerForAbsolutePosition = &object;
    }

    // TODO(pdr): Remove the !object.isLayoutView() condition when removing FrameView
    // paint properties for rootLayerScrolls.
    if (!object.isLayoutView() && object.canContainFixedPositionObjects()) {
        context.transformForFixedPosition = context.currentTransform;
        context.paintOffsetForFixedPosition = context.paintOffset;
        context.clipForFixedPosition = context.currentClip;
    } else if (object.objectPaintProperties() && object.objectPaintProperties()->cssClip()) {
        // CSS clip applies to all descendants, even if this object is not a containing block
        // ancestor of the descendant. It is okay for absolute-position descendants because
        // having CSS clip implies being absolute position container. However for fixed-position
        // descendants we need to insert the clip here if we are not a containing block ancestor
        // of them.
        auto* cssClip = object.objectPaintProperties()->cssClip();

        // Before we actually create anything, check whether in-flow context and fixed-position
        // context has exactly the same clip. Reuse if possible.
        if (context.clipForFixedPosition == cssClip->parent()) {
            context.clipForFixedPosition = cssClip;
            return;
        }

        RefPtr<ClipPaintPropertyNode> clipFixedPosition = ClipPaintPropertyNode::create(
            const_cast<TransformPaintPropertyNode*>(cssClip->localTransformSpace()),
            cssClip->clipRect(),
            context.clipForFixedPosition);
        context.clipForFixedPosition = clipFixedPosition.get();
        object.getMutableForPainting().ensureObjectPaintProperties().setCssClipFixedPosition(clipFixedPosition.release());
    }
}

static void deriveBorderBoxFromContainerContext(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (!object.isBoxModelObject())
        return;

    const LayoutBoxModelObject& boxModelObject = toLayoutBoxModelObject(object);

    switch (object.styleRef().position()) {
    case StaticPosition:
        break;
    case RelativePosition:
        context.paintOffset += boxModelObject.offsetForInFlowPosition();
        break;
    case AbsolutePosition: {
        context.currentTransform = context.transformForAbsolutePosition;
        context.paintOffset = context.paintOffsetForAbsolutePosition;

        // Absolutely positioned content in an inline should be positioned relative to the inline.
        const LayoutObject* container = context.containerForAbsolutePosition;
        if (container && container->isInFlowPositioned() && container->isLayoutInline()) {
            DCHECK(object.isBox());
            context.paintOffset += toLayoutInline(container)->offsetForInFlowPositionedInline(toLayoutBox(object));
        }

        context.currentClip = context.clipForAbsolutePosition;
        break;
    }
    case StickyPosition:
        context.paintOffset += boxModelObject.offsetForInFlowPosition();
        break;
    case FixedPosition:
        context.currentTransform = context.transformForFixedPosition;
        context.paintOffset = context.paintOffsetForFixedPosition;
        context.currentClip = context.clipForFixedPosition;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    if (boxModelObject.isBox() && (!boxModelObject.isSVG() || boxModelObject.isSVGRoot())) {
        // TODO(pdr): Several calls in this function walk back up the tree to calculate containers
        // (e.g., topLeftLocation, offsetForInFlowPosition*). The containing block and other
        // containers can be stored on PaintPropertyTreeBuilderContext instead of recomputing them.
        context.paintOffset.moveBy(toLayoutBox(boxModelObject).topLeftLocation());
        // This is a weird quirk that table cells paint as children of table rows,
        // but their location have the row's location baked-in.
        // Similar adjustment is done in LayoutTableCell::offsetFromContainer().
        if (boxModelObject.isTableCell()) {
            LayoutObject* parentRow = boxModelObject.parent();
            ASSERT(parentRow && parentRow->isTableRow());
            context.paintOffset.moveBy(-toLayoutBox(parentRow)->topLeftLocation());
        }
    }
}

void PaintPropertyTreeBuilder::buildTreeNodes(const LayoutObject& object, PaintPropertyTreeBuilderContext& context)
{
    if (!object.isBoxModelObject() && !object.isSVG())
        return;

    object.getMutableForPainting().clearObjectPaintProperties();

    deriveBorderBoxFromContainerContext(object, context);

    updatePaintOffsetTranslation(object, context);
    updateTransform(object, context);
    updateEffect(object, context);
    updateCssClip(object, context);
    updateLocalBorderBoxContext(object, context);
    updateScrollbarPaintOffset(object, context);
    updateOverflowClip(object, context);
    // TODO(trchen): Insert flattening transform here, as specified by
    // http://www.w3.org/TR/css3-transforms/#transform-style-property
    updatePerspective(object, context);
    updateSvgLocalToBorderBoxTransform(object, context);
    updateScrollTranslation(object, context);
    updateOutOfFlowContext(object, context);
}

} // namespace blink
