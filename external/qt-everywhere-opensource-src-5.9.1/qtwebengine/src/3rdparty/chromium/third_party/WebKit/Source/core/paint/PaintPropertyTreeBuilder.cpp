// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintPropertyTreeBuilder.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutView.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/paint/FindPropertiesNeedingUpdate.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/SVGRootPainter.h"
#include "platform/transforms/TransformationMatrix.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

PaintPropertyTreeBuilderContext
PaintPropertyTreeBuilder::setupInitialContext() {
  PaintPropertyTreeBuilderContext context;

  context.current.clip = context.absolutePosition.clip =
      context.fixedPosition.clip = ClipPaintPropertyNode::root();
  context.currentEffect = EffectPaintPropertyNode::root();
  context.current.transform = context.absolutePosition.transform =
      context.fixedPosition.transform = TransformPaintPropertyNode::root();
  context.current.scroll = context.absolutePosition.scroll =
      context.fixedPosition.scroll = ScrollPaintPropertyNode::root();

  // Ensure scroll tree properties are reset. They will be rebuilt during the
  // tree walk.
  ScrollPaintPropertyNode::root()->clearMainThreadScrollingReasons();

  return context;
}

void updateFrameViewPreTranslation(
    FrameView& frameView,
    PassRefPtr<const TransformPaintPropertyNode> parent,
    const TransformationMatrix& matrix,
    const FloatPoint3D& origin) {
  DCHECK(!RuntimeEnabledFeatures::rootLayerScrollingEnabled());
  if (auto* existingPreTranslation = frameView.preTranslation()) {
    existingPreTranslation->update(std::move(parent), matrix, origin);
  } else {
    frameView.setPreTranslation(
        TransformPaintPropertyNode::create(std::move(parent), matrix, origin));
  }
}

void updateFrameViewContentClip(
    FrameView& frameView,
    PassRefPtr<const ClipPaintPropertyNode> parent,
    PassRefPtr<const TransformPaintPropertyNode> localTransformSpace,
    const FloatRoundedRect& clipRect) {
  DCHECK(!RuntimeEnabledFeatures::rootLayerScrollingEnabled());
  if (auto* existingContentClip = frameView.contentClip()) {
    existingContentClip->update(std::move(parent),
                                std::move(localTransformSpace), clipRect);
  } else {
    frameView.setContentClip(ClipPaintPropertyNode::create(
        std::move(parent), std::move(localTransformSpace), clipRect));
  }
}

void updateFrameViewScrollTranslation(
    FrameView& frameView,
    PassRefPtr<const TransformPaintPropertyNode> parent,
    const TransformationMatrix& matrix,
    const FloatPoint3D& origin) {
  DCHECK(!RuntimeEnabledFeatures::rootLayerScrollingEnabled());
  if (auto* existingScrollTranslation = frameView.scrollTranslation()) {
    existingScrollTranslation->update(std::move(parent), matrix, origin);
  } else {
    frameView.setScrollTranslation(
        TransformPaintPropertyNode::create(std::move(parent), matrix, origin));
  }
}

void updateFrameViewScroll(
    FrameView& frameView,
    PassRefPtr<ScrollPaintPropertyNode> parent,
    PassRefPtr<const TransformPaintPropertyNode> scrollOffset,
    const IntSize& clip,
    const IntSize& bounds,
    bool userScrollableHorizontal,
    bool userScrollableVertical) {
  DCHECK(!RuntimeEnabledFeatures::rootLayerScrollingEnabled());
  if (auto* existingScroll = frameView.scroll()) {
    existingScroll->update(std::move(parent), std::move(scrollOffset), clip,
                           bounds, userScrollableHorizontal,
                           userScrollableVertical);
  } else {
    frameView.setScroll(ScrollPaintPropertyNode::create(
        std::move(parent), std::move(scrollOffset), clip, bounds,
        userScrollableHorizontal, userScrollableVertical));
  }
}

void PaintPropertyTreeBuilder::updateProperties(
    FrameView& frameView,
    PaintPropertyTreeBuilderContext& context) {
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
    // With root layer scrolling, the LayoutView (a LayoutObject) properties are
    // updated like other objects (see updatePropertiesAndContextForSelf and
    // updatePropertiesAndContextForChildren) instead of needing LayoutView-
    // specific property updates here.
    context.current.paintOffset.moveBy(frameView.location());
    context.current.renderingContextID = 0;
    context.current.shouldFlattenInheritedTransform = true;
    context.absolutePosition = context.current;
    context.containerForAbsolutePosition = nullptr;
    context.fixedPosition = context.current;
    return;
  }

#if DCHECK_IS_ON()
  FindFrameViewPropertiesNeedingUpdateScope checkNeedsUpdateScope(&frameView);
#endif

  if (frameView.needsPaintPropertyUpdate()) {
    TransformationMatrix frameTranslate;
    frameTranslate.translate(frameView.x() + context.current.paintOffset.x(),
                             frameView.y() + context.current.paintOffset.y());
    updateFrameViewPreTranslation(frameView, context.current.transform,
                                  frameTranslate, FloatPoint3D());

    FloatRoundedRect contentClip(
        IntRect(IntPoint(), frameView.visibleContentSize()));
    updateFrameViewContentClip(frameView, context.current.clip,
                               frameView.preTranslation(), contentClip);

    ScrollOffset scrollOffset = frameView.scrollOffset();
    if (frameView.isScrollable() || !scrollOffset.isZero()) {
      TransformationMatrix frameScroll;
      frameScroll.translate(-scrollOffset.width(), -scrollOffset.height());
      updateFrameViewScrollTranslation(frameView, frameView.preTranslation(),
                                       frameScroll, FloatPoint3D());

      IntSize scrollClip = frameView.visibleContentSize();
      IntSize scrollBounds = frameView.contentsSize();
      bool userScrollableHorizontal =
          frameView.userInputScrollable(HorizontalScrollbar);
      bool userScrollableVertical =
          frameView.userInputScrollable(VerticalScrollbar);
      updateFrameViewScroll(frameView, context.current.scroll,
                            frameView.scrollTranslation(), scrollClip,
                            scrollBounds, userScrollableHorizontal,
                            userScrollableVertical);
    } else {
      // Ensure pre-existing properties are cleared when there is no scrolling.
      frameView.setScrollTranslation(nullptr);
      frameView.setScroll(nullptr);
    }
  }

  // Initialize the context for current, absolute and fixed position cases.
  // They are the same, except that scroll translation does not apply to
  // fixed position descendants.
  const auto* fixedTransformNode = frameView.preTranslation()
                                       ? frameView.preTranslation()
                                       : context.current.transform;
  auto* fixedScrollNode = context.current.scroll;
  DCHECK(frameView.preTranslation());
  context.current.transform = frameView.preTranslation();
  DCHECK(frameView.contentClip());
  context.current.clip = frameView.contentClip();
  if (const auto* scrollTranslation = frameView.scrollTranslation())
    context.current.transform = scrollTranslation;
  if (auto* scroll = frameView.scroll())
    context.current.scroll = scroll;
  context.current.paintOffset = LayoutPoint();
  context.current.renderingContextID = 0;
  context.current.shouldFlattenInheritedTransform = true;
  context.absolutePosition = context.current;
  context.containerForAbsolutePosition = nullptr;
  context.fixedPosition = context.current;
  context.fixedPosition.transform = fixedTransformNode;
  context.fixedPosition.scroll = fixedScrollNode;

  std::unique_ptr<PropertyTreeState> contentsState(
      new PropertyTreeState(context.current.transform, context.current.clip,
                            context.currentEffect, context.current.scroll));
  frameView.setTotalPropertyTreeStateForContents(std::move(contentsState));
}

void PaintPropertyTreeBuilder::updatePaintOffsetTranslation(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  bool usesPaintOffsetTranslation = false;
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled() &&
      object.isLayoutView()) {
    // Root layer scrolling always creates a translation node for LayoutView to
    // ensure fixed and absolute contexts use the correct transform space.
    usesPaintOffsetTranslation = true;
  } else if (object.isBoxModelObject() &&
             context.current.paintOffset != LayoutPoint()) {
    // TODO(trchen): Eliminate PaintLayer dependency.
    PaintLayer* layer = toLayoutBoxModelObject(object).layer();
    if (layer && layer->paintsWithTransform(GlobalPaintNormalPhase))
      usesPaintOffsetTranslation = true;
  }

  // We should use the same subpixel paint offset values for snapping
  // regardless of whether a transform is present. If there is a transform
  // we round the paint offset but keep around the residual fractional
  // component for the transformed content to paint with.  In spv1 this was
  // called "subpixel accumulation". For more information, see
  // PaintLayer::subpixelAccumulation() and
  // PaintLayerPainter::paintFragmentByApplyingTransform.
  IntPoint roundedPaintOffset = roundedIntPoint(context.current.paintOffset);
  LayoutPoint fractionalPaintOffset =
      LayoutPoint(context.current.paintOffset - roundedPaintOffset);

  if (object.needsPaintPropertyUpdate()) {
    if (usesPaintOffsetTranslation) {
      object.getMutableForPainting()
          .ensurePaintProperties()
          .updatePaintOffsetTranslation(
              context.current.transform,
              TransformationMatrix().translate(roundedPaintOffset.x(),
                                               roundedPaintOffset.y()),
              FloatPoint3D(), context.current.shouldFlattenInheritedTransform,
              context.current.renderingContextID);
    } else {
      if (auto* properties = object.getMutableForPainting().paintProperties())
        properties->clearPaintOffsetTranslation();
    }
  }

  const auto* properties = object.paintProperties();
  if (properties && properties->paintOffsetTranslation()) {
    context.current.transform = properties->paintOffsetTranslation();
    context.current.paintOffset = fractionalPaintOffset;
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled() &&
        object.isLayoutView()) {
      context.absolutePosition.transform = properties->paintOffsetTranslation();
      context.fixedPosition.transform = properties->paintOffsetTranslation();
      context.absolutePosition.paintOffset = LayoutPoint();
      context.fixedPosition.paintOffset = LayoutPoint();
    }
  }
}

static FloatPoint3D transformOrigin(const LayoutBox& box) {
  const ComputedStyle& style = box.styleRef();
  FloatSize borderBoxSize(box.size());
  return FloatPoint3D(
      floatValueForLength(style.transformOriginX(), borderBoxSize.width()),
      floatValueForLength(style.transformOriginY(), borderBoxSize.height()),
      style.transformOriginZ());
}

// SVG does not use the general transform update of |updateTransform|, instead
// creating a transform node for SVG-specific transforms without 3D.
void PaintPropertyTreeBuilder::updateTransformForNonRootSVG(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  DCHECK(object.isSVG() && !object.isSVGRoot());
  // SVG (other than SVGForeignObject) does not use paint offset internally.
  DCHECK(object.isSVGForeignObject() ||
         context.current.paintOffset == LayoutPoint());

  if (object.needsPaintPropertyUpdate()) {
    // TODO(pdr): Refactor this so all non-root SVG objects use the same
    // transform function.
    const AffineTransform& transform = object.isSVGForeignObject()
                                           ? object.localSVGTransform()
                                           : object.localToSVGParentTransform();
    // TODO(pdr): Check for the presence of a transform instead of the value.
    // Checking for an identity matrix will cause the property tree structure
    // to change during animations if the animation passes through the
    // identity matrix.
    if (!transform.isIdentity()) {
      // The origin is included in the local transform, so leave origin empty.
      object.getMutableForPainting().ensurePaintProperties().updateTransform(
          context.current.transform, TransformationMatrix(transform),
          FloatPoint3D());
    } else {
      if (auto* properties = object.getMutableForPainting().paintProperties())
        properties->clearTransform();
    }
  }

  if (object.paintProperties() && object.paintProperties()->transform()) {
    context.current.transform = object.paintProperties()->transform();
    context.current.shouldFlattenInheritedTransform = false;
    context.current.renderingContextID = 0;
  }
}

void PaintPropertyTreeBuilder::updateTransform(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (object.isSVG() && !object.isSVGRoot()) {
    updateTransformForNonRootSVG(object, context);
    return;
  }

  if (object.needsPaintPropertyUpdate()) {
    const ComputedStyle& style = object.styleRef();
    if (object.isBox() && (style.hasTransform() || style.preserves3D())) {
      TransformationMatrix matrix;
      style.applyTransform(
          matrix, toLayoutBox(object).size(),
          ComputedStyle::ExcludeTransformOrigin,
          ComputedStyle::IncludeMotionPath,
          ComputedStyle::IncludeIndependentTransformProperties);

      // TODO(trchen): transform-style should only be respected if a PaintLayer
      // is created.
      // If a node with transform-style: preserve-3d does not exist in an
      // existing rendering context, it establishes a new one.
      unsigned renderingContextID = context.current.renderingContextID;
      if (style.preserves3D() && !renderingContextID)
        renderingContextID = PtrHash<const LayoutObject>::hash(&object);

      object.getMutableForPainting().ensurePaintProperties().updateTransform(
          context.current.transform, matrix,
          transformOrigin(toLayoutBox(object)),
          context.current.shouldFlattenInheritedTransform, renderingContextID);
    } else {
      if (auto* properties = object.getMutableForPainting().paintProperties())
        properties->clearTransform();
    }
  }

  const auto* properties = object.paintProperties();
  if (properties && properties->transform()) {
    context.current.transform = properties->transform();
    if (object.styleRef().preserves3D()) {
      context.current.renderingContextID =
          properties->transform()->renderingContextID();
      context.current.shouldFlattenInheritedTransform = false;
    } else {
      context.current.renderingContextID = 0;
      context.current.shouldFlattenInheritedTransform = true;
    }
  }
}

void PaintPropertyTreeBuilder::updateEffect(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  const ComputedStyle& style = object.styleRef();

  if (!style.isStackingContext()) {
    if (object.needsPaintPropertyUpdate()) {
      if (auto* properties = object.getMutableForPainting().paintProperties())
        properties->clearEffect();
    }
    return;
  }

  // TODO(trchen): Can't omit effect node if we have 3D children.
  // TODO(trchen): Can't omit effect node if we have blending children.
  if (object.needsPaintPropertyUpdate()) {
    bool effectNodeNeeded = false;

    float opacity = style.opacity();
    if (opacity != 1.0f)
      effectNodeNeeded = true;

    CompositorFilterOperations filter;
    if (object.isSVG() && !object.isSVGRoot()) {
      // TODO(trchen): SVG caches filters in SVGResources. Implement it.
    } else if (PaintLayer* layer = toLayoutBoxModelObject(object).layer()) {
      // TODO(trchen): Eliminate PaintLayer dependency.
      filter = layer->createCompositorFilterOperationsForFilter(style);
    }

    const ClipPaintPropertyNode* outputClip = ClipPaintPropertyNode::root();
    // The CSS filter spec didn't specify how filters interact with overflow
    // clips. The implementation here mimics the old Blink/WebKit behavior for
    // backward compatibility.
    // Basically the output of the filter will be affected by clips that applies
    // to the current element. The descendants that paints into the input of the
    // filter ignores any clips collected so far. For example:
    // <div style="overflow:scroll">
    //   <div style="filter:blur(1px);">
    //     <div>A</div>
    //     <div style="position:absolute;">B</div>
    //   </div>
    // </div>
    // In this example "A" should be clipped if the filter was not present.
    // With the filter, "A" will be rastered without clipping, but instead
    // the blurred result will be clipped.
    // On the other hand, "B" should not be clipped because the overflow clip is
    // not in its containing block chain, but as the filter output will be
    // clipped, so a blurred "B" may still be invisible.
    if (!filter.isEmpty()) {
      effectNodeNeeded = true;
      outputClip = context.current.clip;

      // TODO(trchen): A filter may contain spatial operations such that an
      // output pixel may depend on an input pixel outside of the output clip.
      // We should generate a special clip node to represent this expansion.
    }

    if (effectNodeNeeded) {
      object.getMutableForPainting().ensurePaintProperties().updateEffect(
          context.currentEffect, context.current.transform, outputClip,
          std::move(filter), opacity);
    } else {
      if (auto* properties = object.getMutableForPainting().paintProperties())
        properties->clearEffect();
    }
  }

  const auto* properties = object.paintProperties();
  if (properties && properties->effect()) {
    context.currentEffect = properties->effect();
    // TODO(pdr): Once the expansion clip node is created above, it should be
    // used here to update all current clip nodes;
    const ClipPaintPropertyNode* expansionHint = context.current.clip;
    context.current.clip = context.absolutePosition.clip =
        context.fixedPosition.clip = expansionHint;
  }
}

void PaintPropertyTreeBuilder::updateCssClip(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (object.needsPaintPropertyUpdate()) {
    if (object.hasClip()) {
      // Create clip node for descendants that are not fixed position.
      // We don't have to setup context.absolutePosition.clip here because this
      // object must be a container for absolute position descendants, and will
      // copy from in-flow context later at updateOutOfFlowContext() step.
      DCHECK(object.canContainAbsolutePositionObjects());
      LayoutRect clipRect =
          toLayoutBox(object).clipRect(context.current.paintOffset);
      object.getMutableForPainting().ensurePaintProperties().updateCssClip(
          context.current.clip, context.current.transform,
          FloatRoundedRect(FloatRect(clipRect)));
    } else {
      if (auto* properties = object.getMutableForPainting().paintProperties())
        properties->clearCssClip();
    }
  }

  const auto* properties = object.paintProperties();
  if (properties && properties->cssClip())
    context.current.clip = properties->cssClip();
}

void PaintPropertyTreeBuilder::updateLocalBorderBoxContext(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (!object.needsPaintPropertyUpdate())
    return;

  // Avoid adding an ObjectPaintProperties for non-boxes to save memory, since
  // we don't need them at the moment.
  if (!object.isBox() && !object.hasLayer()) {
    if (auto* properties = object.getMutableForPainting().paintProperties())
      properties->clearLocalBorderBoxProperties();
  } else {
    std::unique_ptr<ObjectPaintProperties::PropertyTreeStateWithOffset>
        borderBoxContext =
            wrapUnique(new ObjectPaintProperties::PropertyTreeStateWithOffset(
                context.current.paintOffset,
                PropertyTreeState(context.current.transform,
                                  context.current.clip, context.currentEffect,
                                  context.current.scroll)));
    object.getMutableForPainting()
        .ensurePaintProperties()
        .setLocalBorderBoxProperties(std::move(borderBoxContext));
  }
}

// TODO(trchen): Remove this once we bake the paint offset into frameRect.
void PaintPropertyTreeBuilder::updateScrollbarPaintOffset(
    const LayoutObject& object,
    const PaintPropertyTreeBuilderContext& context) {
  if (!object.needsPaintPropertyUpdate())
    return;

  bool needsScrollbarPaintOffset = false;
  IntPoint roundedPaintOffset = roundedIntPoint(context.current.paintOffset);
  if (roundedPaintOffset != IntPoint() && object.isBoxModelObject()) {
    if (auto* area = toLayoutBoxModelObject(object).getScrollableArea()) {
      if (area->horizontalScrollbar() || area->verticalScrollbar()) {
        auto paintOffset = TransformationMatrix().translate(
            roundedPaintOffset.x(), roundedPaintOffset.y());
        object.getMutableForPainting()
            .ensurePaintProperties()
            .updateScrollbarPaintOffset(context.current.transform, paintOffset,
                                        FloatPoint3D());
        needsScrollbarPaintOffset = true;
      }
    }
  }

  auto* properties = object.getMutableForPainting().paintProperties();
  if (!needsScrollbarPaintOffset && properties)
    properties->clearScrollbarPaintOffset();
}

void PaintPropertyTreeBuilder::updateMainThreadScrollingReasons(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  // TODO(pdr): Mark properties as needing an update for main thread scroll
  // reasons and ensure reason changes are propagated to ancestors to account
  // for the parent walk below. https://crbug.com/664672.

  if (context.current.scroll &&
      !object.document().settings()->threadedScrollingEnabled()) {
    context.current.scroll->addMainThreadScrollingReasons(
        MainThreadScrollingReason::kThreadedScrollingDisabled);
  }

  if (object.isBackgroundAttachmentFixedObject()) {
    auto* scrollNode = context.current.scroll;
    while (
        scrollNode &&
        !scrollNode->hasMainThreadScrollingReasons(
            MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects)) {
      scrollNode->addMainThreadScrollingReasons(
          MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);
      scrollNode = scrollNode->parent();
    }
  }
}

void PaintPropertyTreeBuilder::updateOverflowClip(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (!object.isBox())
    return;

  if (object.needsPaintPropertyUpdate()) {
    const LayoutBox& box = toLayoutBox(object);
    // The <input> elements can't have contents thus CSS overflow property
    // doesn't apply.  However for layout purposes we do generate child layout
    // objects for them, e.g. button label.  We should clip the overflow from
    // those children. This is called control clip and we technically treat them
    // like overflow clip.
    LayoutRect clipRect;
    if (box.hasControlClip()) {
      clipRect = box.controlClipRect(context.current.paintOffset);
    } else if (box.hasOverflowClip() || box.styleRef().containsPaint() ||
               (box.isSVGRoot() &&
                toLayoutSVGRoot(box).shouldApplyViewportClip())) {
      clipRect = LayoutRect(pixelSnappedIntRect(
          box.overflowClipRect(context.current.paintOffset)));
    } else {
      if (auto* properties = object.getMutableForPainting().paintProperties()) {
        properties->clearInnerBorderRadiusClip();
        properties->clearOverflowClip();
      }
      return;
    }

    const auto* currentClip = context.current.clip;
    if (box.styleRef().hasBorderRadius()) {
      auto innerBorder = box.styleRef().getRoundedInnerBorderFor(
          LayoutRect(context.current.paintOffset, box.size()));
      object.getMutableForPainting()
          .ensurePaintProperties()
          .updateInnerBorderRadiusClip(context.current.clip,
                                       context.current.transform, innerBorder);
      currentClip = object.paintProperties()->innerBorderRadiusClip();
    } else if (auto* properties =
                   object.getMutableForPainting().paintProperties()) {
      properties->clearInnerBorderRadiusClip();
    }

    object.getMutableForPainting().ensurePaintProperties().updateOverflowClip(
        currentClip, context.current.transform,
        FloatRoundedRect(FloatRect(clipRect)));
  }

  const auto* properties = object.paintProperties();
  if (properties && properties->overflowClip())
    context.current.clip = properties->overflowClip();
}

static FloatPoint perspectiveOrigin(const LayoutBox& box) {
  const ComputedStyle& style = box.styleRef();
  FloatSize borderBoxSize(box.size());
  return FloatPoint(
      floatValueForLength(style.perspectiveOriginX(), borderBoxSize.width()),
      floatValueForLength(style.perspectiveOriginY(), borderBoxSize.height()));
}

void PaintPropertyTreeBuilder::updatePerspective(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (object.needsPaintPropertyUpdate()) {
    const ComputedStyle& style = object.styleRef();
    if (object.isBox() && style.hasPerspective()) {
      // The perspective node must not flatten (else nothing will get
      // perspective), but it should still extend the rendering context as
      // most transform nodes do.
      TransformationMatrix matrix =
          TransformationMatrix().applyPerspective(style.perspective());
      FloatPoint3D origin = perspectiveOrigin(toLayoutBox(object)) +
                            toLayoutSize(context.current.paintOffset);
      object.getMutableForPainting().ensurePaintProperties().updatePerspective(
          context.current.transform, matrix, origin,
          context.current.shouldFlattenInheritedTransform,
          context.current.renderingContextID);
    } else {
      if (auto* properties = object.getMutableForPainting().paintProperties())
        properties->clearPerspective();
    }
  }

  const auto* properties = object.paintProperties();
  if (properties && properties->perspective()) {
    context.current.transform = properties->perspective();
    context.current.shouldFlattenInheritedTransform = false;
  }
}

void PaintPropertyTreeBuilder::updateSvgLocalToBorderBoxTransform(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (!object.isSVGRoot())
    return;

  if (object.needsPaintPropertyUpdate()) {
    AffineTransform transformToBorderBox =
        SVGRootPainter(toLayoutSVGRoot(object))
            .transformToPixelSnappedBorderBox(context.current.paintOffset);
    if (!transformToBorderBox.isIdentity()) {
      object.getMutableForPainting()
          .ensurePaintProperties()
          .updateSvgLocalToBorderBoxTransform(
              context.current.transform, transformToBorderBox, FloatPoint3D());
    } else {
      if (auto* properties = object.getMutableForPainting().paintProperties())
        properties->clearSvgLocalToBorderBoxTransform();
    }
  }

  const auto* properties = object.paintProperties();
  if (properties && properties->svgLocalToBorderBoxTransform()) {
    context.current.transform = properties->svgLocalToBorderBoxTransform();
    context.current.shouldFlattenInheritedTransform = false;
    context.current.renderingContextID = 0;
  }
  // The paint offset is included in |transformToBorderBox| so SVG does not need
  // to handle paint offset internally.
  context.current.paintOffset = LayoutPoint();
}

void PaintPropertyTreeBuilder::updateScrollAndScrollTranslation(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (object.needsPaintPropertyUpdate()) {
    if (object.hasOverflowClip()) {
      const LayoutBox& box = toLayoutBox(object);
      const PaintLayerScrollableArea* scrollableArea = box.getScrollableArea();
      IntSize scrollOffset = box.scrolledContentOffset();
      if (!scrollOffset.isZero() || scrollableArea->scrollsOverflow()) {
        TransformationMatrix matrix = TransformationMatrix().translate(
            -scrollOffset.width(), -scrollOffset.height());
        object.getMutableForPainting()
            .ensurePaintProperties()
            .updateScrollTranslation(
                context.current.transform, matrix, FloatPoint3D(),
                context.current.shouldFlattenInheritedTransform,
                context.current.renderingContextID);

        IntSize scrollClip = scrollableArea->visibleContentRect().size();
        IntSize scrollBounds = scrollableArea->contentsSize();
        bool userScrollableHorizontal =
            scrollableArea->userInputScrollable(HorizontalScrollbar);
        bool userScrollableVertical =
            scrollableArea->userInputScrollable(VerticalScrollbar);
        object.getMutableForPainting().ensurePaintProperties().updateScroll(
            context.current.scroll,
            object.paintProperties()->scrollTranslation(), scrollClip,
            scrollBounds, userScrollableHorizontal, userScrollableVertical);
      } else {
        // Ensure pre-existing properties are cleared when there is no
        // scrolling.
        auto* properties = object.getMutableForPainting().paintProperties();
        if (properties) {
          properties->clearScrollTranslation();
          properties->clearScroll();
        }
      }
    }
  }

  if (object.paintProperties() && object.paintProperties()->scroll()) {
    context.current.transform = object.paintProperties()->scrollTranslation();
    const auto* scroll = object.paintProperties()->scroll();
    // TODO(pdr): Remove this const cast.
    context.current.scroll = const_cast<ScrollPaintPropertyNode*>(scroll);
    context.current.shouldFlattenInheritedTransform = false;
  }
}

void PaintPropertyTreeBuilder::updateOutOfFlowContext(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (object.canContainAbsolutePositionObjects()) {
    context.absolutePosition = context.current;
    context.containerForAbsolutePosition = &object;
  }

  if (object.isLayoutView()) {
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
      const auto* initialFixedTransform = context.fixedPosition.transform;
      auto* initialFixedScroll = context.fixedPosition.scroll;

      context.fixedPosition = context.current;

      // Fixed position transform and scroll nodes should not be affected.
      context.fixedPosition.transform = initialFixedTransform;
      context.fixedPosition.scroll = initialFixedScroll;
    }
  } else if (object.canContainFixedPositionObjects()) {
    context.fixedPosition = context.current;
  } else if (object.getMutableForPainting().paintProperties() &&
             object.paintProperties()->cssClip()) {
    // CSS clip applies to all descendants, even if this object is not a
    // containing block ancestor of the descendant. It is okay for
    // absolute-position descendants because having CSS clip implies being
    // absolute position container. However for fixed-position descendants we
    // need to insert the clip here if we are not a containing block ancestor of
    // them.
    auto* cssClip = object.getMutableForPainting().paintProperties()->cssClip();

    // Before we actually create anything, check whether in-flow context and
    // fixed-position context has exactly the same clip. Reuse if possible.
    if (context.fixedPosition.clip == cssClip->parent()) {
      context.fixedPosition.clip = cssClip;
    } else {
      if (object.needsPaintPropertyUpdate()) {
        object.getMutableForPainting()
            .ensurePaintProperties()
            .updateCssClipFixedPosition(context.fixedPosition.clip,
                                        const_cast<TransformPaintPropertyNode*>(
                                            cssClip->localTransformSpace()),
                                        cssClip->clipRect());
      }
      const auto* properties = object.paintProperties();
      if (properties && properties->cssClipFixedPosition())
        context.fixedPosition.clip = properties->cssClipFixedPosition();
      return;
    }
  }

  if (object.needsPaintPropertyUpdate()) {
    if (auto* properties = object.getMutableForPainting().paintProperties())
      properties->clearCssClipFixedPosition();
  }
}

// Override ContainingBlockContext based on the properties of a containing block
// that was previously walked in a subtree other than the current subtree being
// walked. Used for out-of-flow positioned descendants of multi-column spanner
// when the containing block is not in the normal tree walk order.
// For example:
// <div id="columns" style="columns: 2">
//   <div id="relative" style="position: relative">
//     <div id="spanner" style="column-span: all">
//       <div id="absolute" style="position: absolute"></div>
//     </div>
//   </div>
// <div>
// The real containing block of "absolute" is "relative" which is not in the
// tree-walk order of "columns" -> spanner placeholder -> spanner -> absolute.
// Here we rebuild a ContainingBlockContext based on the properties of
// "relative" for "absolute".
static void overrideContaineringBlockContextFromRealContainingBlock(
    const LayoutBlock& containingBlock,
    PaintPropertyTreeBuilderContext::ContainingBlockContext& context) {
  const auto* properties =
      containingBlock.paintProperties()->localBorderBoxProperties();
  DCHECK(properties);

  context.transform = properties->propertyTreeState.transform();
  context.paintOffset = properties->paintOffset;
  context.shouldFlattenInheritedTransform =
      context.transform && context.transform->flattensInheritedTransform();
  context.renderingContextID =
      context.transform ? context.transform->renderingContextID() : 0;
  context.clip = properties->propertyTreeState.clip();
  context.scroll = const_cast<ScrollPaintPropertyNode*>(
      properties->propertyTreeState.scroll());
}

static void deriveBorderBoxFromContainerContext(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (!object.isBoxModelObject())
    return;

  const LayoutBoxModelObject& boxModelObject = toLayoutBoxModelObject(object);

  switch (object.styleRef().position()) {
    case StaticPosition:
      break;
    case RelativePosition:
      context.current.paintOffset += boxModelObject.offsetForInFlowPosition();
      break;
    case AbsolutePosition: {
      if (context.isUnderMultiColumnSpanner) {
        const LayoutObject* container = boxModelObject.container();
        if (container != context.containerForAbsolutePosition) {
          // The container of the absolute-position is not in the normal tree-
          // walk order.
          context.containerForAbsolutePosition =
              toLayoutBoxModelObject(container);
          // The container is never a LayoutInline. In the example above
          // overrideContaineringBlockContextFromRealContainingBlock(), if we
          // change the container to an inline, there will be an anonymous
          // blocks created because the spanner is always a block.
          overrideContaineringBlockContextFromRealContainingBlock(
              toLayoutBlock(*container), context.current);
        }
      } else {
        DCHECK(context.containerForAbsolutePosition ==
               boxModelObject.container());
        context.current = context.absolutePosition;
      }

      // Absolutely positioned content in an inline should be positioned
      // relative to the inline.
      const LayoutObject* container = context.containerForAbsolutePosition;
      if (container && container->isInFlowPositioned() &&
          container->isLayoutInline()) {
        DCHECK(object.isBox());
        context.current.paintOffset +=
            toLayoutInline(container)->offsetForInFlowPositionedInline(
                toLayoutBox(object));
      }
      break;
    }
    case StickyPosition:
      context.current.paintOffset += boxModelObject.offsetForInFlowPosition();
      break;
    case FixedPosition:
      if (context.isUnderMultiColumnSpanner) {
        // The container of the fixed-position object may or may not be in the
        // normal tree-walk order.
        overrideContaineringBlockContextFromRealContainingBlock(
            toLayoutBlock(*boxModelObject.container()), context.current);
      } else {
        context.current = context.fixedPosition;
      }
      break;
    default:
      ASSERT_NOT_REACHED();
  }

  // SVGForeignObject needs paint offset because its viewport offset is baked
  // into its location(), while its localSVGTransform() doesn't contain the
  // offset.
  if (boxModelObject.isBox() &&
      (!boxModelObject.isSVG() || boxModelObject.isSVGRoot() ||
       boxModelObject.isSVGForeignObject())) {
    // TODO(pdr): Several calls in this function walk back up the tree to
    // calculate containers (e.g., topLeftLocation, offsetForInFlowPosition*).
    // The containing block and other containers can be stored on
    // PaintPropertyTreeBuilderContext instead of recomputing them.
    context.current.paintOffset.moveBy(
        toLayoutBox(boxModelObject).topLeftLocation());
    // This is a weird quirk that table cells paint as children of table rows,
    // but their location have the row's location baked-in.
    // Similar adjustment is done in LayoutTableCell::offsetFromContainer().
    if (boxModelObject.isTableCell()) {
      LayoutObject* parentRow = boxModelObject.parent();
      DCHECK(parentRow && parentRow->isTableRow());
      context.current.paintOffset.moveBy(
          -toLayoutBox(parentRow)->topLeftLocation());
    }
  }
}

void PaintPropertyTreeBuilder::updatePropertiesForSelf(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (!object.isBoxModelObject() && !object.isSVG())
    return;

#if DCHECK_IS_ON()
  FindObjectPropertiesNeedingUpdateScope checkNeedsUpdateScope(object);
#endif

  deriveBorderBoxFromContainerContext(object, context);

  updatePaintOffsetTranslation(object, context);
  updateTransform(object, context);
  updateEffect(object, context);
  updateCssClip(object, context);
  updateLocalBorderBoxContext(object, context);
  updateScrollbarPaintOffset(object, context);
  updateMainThreadScrollingReasons(object, context);
}

void PaintPropertyTreeBuilder::updatePropertiesForChildren(
    const LayoutObject& object,
    PaintPropertyTreeBuilderContext& context) {
  if (!object.isBoxModelObject() && !object.isSVG())
    return;

#if DCHECK_IS_ON()
  FindObjectPropertiesNeedingUpdateScope checkNeedsUpdateScope(object);
#endif

  updateOverflowClip(object, context);
  updatePerspective(object, context);
  updateSvgLocalToBorderBoxTransform(object, context);
  updateScrollAndScrollTranslation(object, context);
  updateOutOfFlowContext(object, context);
}

}  // namespace blink
