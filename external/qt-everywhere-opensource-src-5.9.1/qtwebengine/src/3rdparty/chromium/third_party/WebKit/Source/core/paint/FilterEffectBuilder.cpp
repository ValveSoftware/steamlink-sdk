/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/paint/FilterEffectBuilder.h"

#include "core/style/FilterOperations.h"
#include "core/svg/SVGElementProxy.h"
#include "core/svg/SVGFilterElement.h"
#include "core/svg/SVGLengthContext.h"
#include "core/svg/graphics/filters/SVGFilterBuilder.h"
#include "platform/LengthFunctions.h"
#include "platform/graphics/ColorSpace.h"
#include "platform/graphics/CompositorFilterOperations.h"
#include "platform/graphics/filters/FEBoxReflect.h"
#include "platform/graphics/filters/FEColorMatrix.h"
#include "platform/graphics/filters/FEComponentTransfer.h"
#include "platform/graphics/filters/FEDropShadow.h"
#include "platform/graphics/filters/FEGaussianBlur.h"
#include "platform/graphics/filters/Filter.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/graphics/filters/SourceGraphic.h"
#include "public/platform/WebPoint.h"
#include "wtf/MathExtras.h"
#include <algorithm>

namespace blink {

namespace {

inline void endMatrixRow(Vector<float>& matrix) {
  matrix.uncheckedAppend(0);
  matrix.uncheckedAppend(0);
}

inline void lastMatrixRow(Vector<float>& matrix) {
  matrix.uncheckedAppend(0);
  matrix.uncheckedAppend(0);
  matrix.uncheckedAppend(0);
  matrix.uncheckedAppend(1);
  matrix.uncheckedAppend(0);
}

Vector<float> grayscaleMatrix(double amount) {
  double oneMinusAmount = clampTo(1 - amount, 0.0, 1.0);

  // See
  // https://dvcs.w3.org/hg/FXTF/raw-file/tip/filters/index.html#grayscaleEquivalent
  // for information on parameters.
  Vector<float> matrix;
  matrix.reserveInitialCapacity(20);

  matrix.uncheckedAppend(clampTo<float>(0.2126 + 0.7874 * oneMinusAmount));
  matrix.uncheckedAppend(clampTo<float>(0.7152 - 0.7152 * oneMinusAmount));
  matrix.uncheckedAppend(clampTo<float>(0.0722 - 0.0722 * oneMinusAmount));
  endMatrixRow(matrix);

  matrix.uncheckedAppend(clampTo<float>(0.2126 - 0.2126 * oneMinusAmount));
  matrix.uncheckedAppend(clampTo<float>(0.7152 + 0.2848 * oneMinusAmount));
  matrix.uncheckedAppend(clampTo<float>(0.0722 - 0.0722 * oneMinusAmount));
  endMatrixRow(matrix);

  matrix.uncheckedAppend(clampTo<float>(0.2126 - 0.2126 * oneMinusAmount));
  matrix.uncheckedAppend(clampTo<float>(0.7152 - 0.7152 * oneMinusAmount));
  matrix.uncheckedAppend(clampTo<float>(0.0722 + 0.9278 * oneMinusAmount));
  endMatrixRow(matrix);

  lastMatrixRow(matrix);
  return matrix;
}

Vector<float> sepiaMatrix(double amount) {
  double oneMinusAmount = clampTo(1 - amount, 0.0, 1.0);

  // See
  // https://dvcs.w3.org/hg/FXTF/raw-file/tip/filters/index.html#sepiaEquivalent
  // for information on parameters.
  Vector<float> matrix;
  matrix.reserveInitialCapacity(20);

  matrix.uncheckedAppend(clampTo<float>(0.393 + 0.607 * oneMinusAmount));
  matrix.uncheckedAppend(clampTo<float>(0.769 - 0.769 * oneMinusAmount));
  matrix.uncheckedAppend(clampTo<float>(0.189 - 0.189 * oneMinusAmount));
  endMatrixRow(matrix);

  matrix.uncheckedAppend(clampTo<float>(0.349 - 0.349 * oneMinusAmount));
  matrix.uncheckedAppend(clampTo<float>(0.686 + 0.314 * oneMinusAmount));
  matrix.uncheckedAppend(clampTo<float>(0.168 - 0.168 * oneMinusAmount));
  endMatrixRow(matrix);

  matrix.uncheckedAppend(clampTo<float>(0.272 - 0.272 * oneMinusAmount));
  matrix.uncheckedAppend(clampTo<float>(0.534 - 0.534 * oneMinusAmount));
  matrix.uncheckedAppend(clampTo<float>(0.131 + 0.869 * oneMinusAmount));
  endMatrixRow(matrix);

  lastMatrixRow(matrix);
  return matrix;
}

}  // namespace

FilterEffectBuilder::FilterEffectBuilder(Node* target,
                                         const FloatRect& zoomedReferenceBox,
                                         float zoom,
                                         const SkPaint* fillPaint,
                                         const SkPaint* strokePaint)
    : m_targetContext(target),
      m_referenceBox(zoomedReferenceBox),
      m_zoom(zoom),
      m_fillPaint(fillPaint),
      m_strokePaint(strokePaint) {
  if (m_zoom != 1)
    m_referenceBox.scale(1 / m_zoom);
}

FilterEffect* FilterEffectBuilder::buildFilterEffect(
    const FilterOperations& operations) const {
  // Create a parent filter for shorthand filters. These have already been
  // scaled by the CSS code for page zoom, so scale is 1.0 here.
  Filter* parentFilter = Filter::create(1.0f);
  FilterEffect* previousEffect = parentFilter->getSourceGraphic();
  for (FilterOperation* filterOperation : operations.operations()) {
    FilterEffect* effect = nullptr;
    switch (filterOperation->type()) {
      case FilterOperation::REFERENCE: {
        ReferenceFilterOperation& referenceOperation =
            toReferenceFilterOperation(*filterOperation);
        Filter* referenceFilter =
            buildReferenceFilter(referenceOperation, previousEffect);
        if (referenceFilter) {
          effect = referenceFilter->lastEffect();
          // TODO(fs): This is essentially only needed for the
          // side-effects (mapRect). The filter differs from the one
          // computed just above in what the SourceGraphic is, and how
          // it's connected to the filter-chain.
          referenceFilter = buildReferenceFilter(referenceOperation, nullptr);
        }
        referenceOperation.setFilter(referenceFilter);
        break;
      }
      case FilterOperation::GRAYSCALE: {
        Vector<float> inputParameters = grayscaleMatrix(
            toBasicColorMatrixFilterOperation(filterOperation)->amount());
        effect = FEColorMatrix::create(parentFilter, FECOLORMATRIX_TYPE_MATRIX,
                                       inputParameters);
        break;
      }
      case FilterOperation::SEPIA: {
        Vector<float> inputParameters = sepiaMatrix(
            toBasicColorMatrixFilterOperation(filterOperation)->amount());
        effect = FEColorMatrix::create(parentFilter, FECOLORMATRIX_TYPE_MATRIX,
                                       inputParameters);
        break;
      }
      case FilterOperation::SATURATE: {
        Vector<float> inputParameters;
        inputParameters.append(clampTo<float>(
            toBasicColorMatrixFilterOperation(filterOperation)->amount()));
        effect = FEColorMatrix::create(
            parentFilter, FECOLORMATRIX_TYPE_SATURATE, inputParameters);
        break;
      }
      case FilterOperation::HUE_ROTATE: {
        Vector<float> inputParameters;
        inputParameters.append(clampTo<float>(
            toBasicColorMatrixFilterOperation(filterOperation)->amount()));
        effect = FEColorMatrix::create(
            parentFilter, FECOLORMATRIX_TYPE_HUEROTATE, inputParameters);
        break;
      }
      case FilterOperation::INVERT: {
        BasicComponentTransferFilterOperation* componentTransferOperation =
            toBasicComponentTransferFilterOperation(filterOperation);
        ComponentTransferFunction transferFunction;
        transferFunction.type = FECOMPONENTTRANSFER_TYPE_TABLE;
        Vector<float> transferParameters;
        transferParameters.append(
            clampTo<float>(componentTransferOperation->amount()));
        transferParameters.append(
            clampTo<float>(1 - componentTransferOperation->amount()));
        transferFunction.tableValues = transferParameters;

        ComponentTransferFunction nullFunction;
        effect = FEComponentTransfer::create(parentFilter, transferFunction,
                                             transferFunction, transferFunction,
                                             nullFunction);
        break;
      }
      case FilterOperation::OPACITY: {
        ComponentTransferFunction transferFunction;
        transferFunction.type = FECOMPONENTTRANSFER_TYPE_TABLE;
        Vector<float> transferParameters;
        transferParameters.append(0);
        transferParameters.append(clampTo<float>(
            toBasicComponentTransferFilterOperation(filterOperation)
                ->amount()));
        transferFunction.tableValues = transferParameters;

        ComponentTransferFunction nullFunction;
        effect = FEComponentTransfer::create(parentFilter, nullFunction,
                                             nullFunction, nullFunction,
                                             transferFunction);
        break;
      }
      case FilterOperation::BRIGHTNESS: {
        ComponentTransferFunction transferFunction;
        transferFunction.type = FECOMPONENTTRANSFER_TYPE_LINEAR;
        transferFunction.slope = clampTo<float>(
            toBasicComponentTransferFilterOperation(filterOperation)->amount());
        transferFunction.intercept = 0;

        ComponentTransferFunction nullFunction;
        effect = FEComponentTransfer::create(parentFilter, transferFunction,
                                             transferFunction, transferFunction,
                                             nullFunction);
        break;
      }
      case FilterOperation::CONTRAST: {
        ComponentTransferFunction transferFunction;
        transferFunction.type = FECOMPONENTTRANSFER_TYPE_LINEAR;
        float amount = clampTo<float>(
            toBasicComponentTransferFilterOperation(filterOperation)->amount());
        transferFunction.slope = amount;
        transferFunction.intercept = -0.5 * amount + 0.5;

        ComponentTransferFunction nullFunction;
        effect = FEComponentTransfer::create(parentFilter, transferFunction,
                                             transferFunction, transferFunction,
                                             nullFunction);
        break;
      }
      case FilterOperation::BLUR: {
        float stdDeviation = floatValueForLength(
            toBlurFilterOperation(filterOperation)->stdDeviation(), 0);
        effect =
            FEGaussianBlur::create(parentFilter, stdDeviation, stdDeviation);
        break;
      }
      case FilterOperation::DROP_SHADOW: {
        DropShadowFilterOperation* dropShadowOperation =
            toDropShadowFilterOperation(filterOperation);
        float stdDeviation = dropShadowOperation->stdDeviation();
        float x = dropShadowOperation->x();
        float y = dropShadowOperation->y();
        effect = FEDropShadow::create(parentFilter, stdDeviation, stdDeviation,
                                      x, y, dropShadowOperation->getColor(), 1);
        break;
      }
      case FilterOperation::BOX_REFLECT: {
        BoxReflectFilterOperation* boxReflectOperation =
            toBoxReflectFilterOperation(filterOperation);
        effect = FEBoxReflect::create(parentFilter,
                                      boxReflectOperation->reflection());
        break;
      }
      default:
        break;
    }

    if (effect) {
      if (filterOperation->type() != FilterOperation::REFERENCE) {
        // Unlike SVG, filters applied here should not clip to their primitive
        // subregions.
        effect->setClipsToBounds(false);
        effect->setOperatingColorSpace(ColorSpaceDeviceRGB);
        effect->inputEffects().append(previousEffect);
      }
      if (previousEffect->originTainted())
        effect->setOriginTainted();
      previousEffect = effect;
    }
  }
  return previousEffect;
}

CompositorFilterOperations FilterEffectBuilder::buildFilterOperations(
    const FilterOperations& operations) const {
  ColorSpace currentColorSpace = ColorSpaceDeviceRGB;

  CompositorFilterOperations filters;
  for (FilterOperation* op : operations.operations()) {
    switch (op->type()) {
      case FilterOperation::REFERENCE: {
        ReferenceFilterOperation& referenceOperation =
            toReferenceFilterOperation(*op);
        Filter* referenceFilter =
            buildReferenceFilter(referenceOperation, nullptr);
        if (referenceFilter && referenceFilter->lastEffect()) {
          SkiaImageFilterBuilder::populateSourceGraphicImageFilters(
              referenceFilter->getSourceGraphic(), nullptr, currentColorSpace);

          FilterEffect* filterEffect = referenceFilter->lastEffect();
          currentColorSpace = filterEffect->operatingColorSpace();
          filters.appendReferenceFilter(
              SkiaImageFilterBuilder::build(filterEffect, currentColorSpace));
        }
        referenceOperation.setFilter(referenceFilter);
        break;
      }
      case FilterOperation::GRAYSCALE:
      case FilterOperation::SEPIA:
      case FilterOperation::SATURATE:
      case FilterOperation::HUE_ROTATE: {
        float amount = toBasicColorMatrixFilterOperation(*op).amount();
        switch (op->type()) {
          case FilterOperation::GRAYSCALE:
            filters.appendGrayscaleFilter(amount);
            break;
          case FilterOperation::SEPIA:
            filters.appendSepiaFilter(amount);
            break;
          case FilterOperation::SATURATE:
            filters.appendSaturateFilter(amount);
            break;
          case FilterOperation::HUE_ROTATE:
            filters.appendHueRotateFilter(amount);
            break;
          default:
            NOTREACHED();
        }
        break;
      }
      case FilterOperation::INVERT:
      case FilterOperation::OPACITY:
      case FilterOperation::BRIGHTNESS:
      case FilterOperation::CONTRAST: {
        float amount = toBasicComponentTransferFilterOperation(*op).amount();
        switch (op->type()) {
          case FilterOperation::INVERT:
            filters.appendInvertFilter(amount);
            break;
          case FilterOperation::OPACITY:
            filters.appendOpacityFilter(amount);
            break;
          case FilterOperation::BRIGHTNESS:
            filters.appendBrightnessFilter(amount);
            break;
          case FilterOperation::CONTRAST:
            filters.appendContrastFilter(amount);
            break;
          default:
            NOTREACHED();
        }
        break;
      }
      case FilterOperation::BLUR: {
        float pixelRadius =
            toBlurFilterOperation(*op).stdDeviation().getFloatValue();
        filters.appendBlurFilter(pixelRadius);
        break;
      }
      case FilterOperation::DROP_SHADOW: {
        const DropShadowFilterOperation& drop =
            toDropShadowFilterOperation(*op);
        filters.appendDropShadowFilter(WebPoint(drop.x(), drop.y()),
                                       drop.stdDeviation(),
                                       drop.getColor().rgb());
        break;
      }
      case FilterOperation::BOX_REFLECT: {
        // TODO(jbroman): Consider explaining box reflect to the compositor,
        // instead of calling this a "reference filter".
        const auto& reflection = toBoxReflectFilterOperation(*op).reflection();
        filters.appendReferenceFilter(
            SkiaImageFilterBuilder::buildBoxReflectFilter(reflection, nullptr));
        break;
      }
      case FilterOperation::NONE:
        break;
    }
  }
  if (currentColorSpace != ColorSpaceDeviceRGB) {
    // Transform to device color space at the end of processing, if required.
    sk_sp<SkImageFilter> filter = SkiaImageFilterBuilder::transformColorSpace(
        nullptr, currentColorSpace, ColorSpaceDeviceRGB);
    filters.appendReferenceFilter(std::move(filter));
  }
  return filters;
}

Filter* FilterEffectBuilder::buildReferenceFilter(
    const ReferenceFilterOperation& referenceOperation,
    FilterEffect* previousEffect) const {
  DCHECK(m_targetContext);
  Element* filterElement = referenceOperation.elementProxy().findElement(
      m_targetContext->treeScope());
  if (!isSVGFilterElement(filterElement))
    return nullptr;
  return buildReferenceFilter(toSVGFilterElement(*filterElement),
                              previousEffect);
}

Filter* FilterEffectBuilder::buildReferenceFilter(
    SVGFilterElement& filterElement,
    FilterEffect* previousEffect,
    SVGFilterGraphNodeMap* nodeMap) const {
  FloatRect filterRegion = SVGLengthContext::resolveRectangle<SVGFilterElement>(
      &filterElement, filterElement.filterUnits()->currentValue()->enumValue(),
      m_referenceBox);
  // TODO(fs): We rely on the presence of a node map here to opt-in to the
  // check for an empty filter region. The reason for this is that we lack a
  // viewport to resolve against for HTML content. This is crbug.com/512453.
  if (nodeMap && filterRegion.isEmpty())
    return nullptr;

  bool primitiveBoundingBoxMode =
      filterElement.primitiveUnits()->currentValue()->enumValue() ==
      SVGUnitTypes::kSvgUnitTypeObjectboundingbox;
  Filter::UnitScaling unitScaling =
      primitiveBoundingBoxMode ? Filter::BoundingBox : Filter::UserSpace;
  Filter* result =
      Filter::create(m_referenceBox, filterRegion, m_zoom, unitScaling);
  if (!previousEffect)
    previousEffect = result->getSourceGraphic();
  SVGFilterBuilder builder(previousEffect, nodeMap, m_fillPaint, m_strokePaint);
  builder.buildGraph(result, filterElement, m_referenceBox);
  result->setLastEffect(builder.lastEffect());
  return result;
}

}  // namespace blink
