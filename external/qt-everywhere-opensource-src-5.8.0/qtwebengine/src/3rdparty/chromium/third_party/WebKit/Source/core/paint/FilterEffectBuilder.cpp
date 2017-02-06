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

#include "core/layout/svg/ReferenceFilterBuilder.h"
#include "core/paint/PaintLayer.h"
#include "core/svg/SVGFilterElement.h"
#include "core/svg/SVGLengthContext.h"
#include "core/svg/graphics/filters/SVGFilterBuilder.h"
#include "platform/FloatConversion.h"
#include "platform/LengthFunctions.h"
#include "platform/graphics/ColorSpace.h"
#include "platform/graphics/filters/FEBoxReflect.h"
#include "platform/graphics/filters/FEColorMatrix.h"
#include "platform/graphics/filters/FEComponentTransfer.h"
#include "platform/graphics/filters/FEDropShadow.h"
#include "platform/graphics/filters/FEGaussianBlur.h"
#include "platform/graphics/filters/Filter.h"
#include "platform/graphics/filters/FilterOperations.h"
#include "platform/graphics/filters/SourceGraphic.h"
#include "wtf/MathExtras.h"
#include <algorithm>

namespace blink {

namespace {

inline void endMatrixRow(Vector<float>& matrix)
{
    matrix.uncheckedAppend(0);
    matrix.uncheckedAppend(0);
}

inline void lastMatrixRow(Vector<float>& matrix)
{
    matrix.uncheckedAppend(0);
    matrix.uncheckedAppend(0);
    matrix.uncheckedAppend(0);
    matrix.uncheckedAppend(1);
    matrix.uncheckedAppend(0);
}

Vector<float> grayscaleMatrix(double amount)
{
    double oneMinusAmount = clampTo(1 - amount, 0.0, 1.0);

    // See https://dvcs.w3.org/hg/FXTF/raw-file/tip/filters/index.html#grayscaleEquivalent
    // for information on parameters.
    Vector<float> matrix;
    matrix.reserveInitialCapacity(20);

    matrix.uncheckedAppend(narrowPrecisionToFloat(0.2126 + 0.7874 * oneMinusAmount));
    matrix.uncheckedAppend(narrowPrecisionToFloat(0.7152 - 0.7152 * oneMinusAmount));
    matrix.uncheckedAppend(narrowPrecisionToFloat(0.0722 - 0.0722 * oneMinusAmount));
    endMatrixRow(matrix);

    matrix.uncheckedAppend(narrowPrecisionToFloat(0.2126 - 0.2126 * oneMinusAmount));
    matrix.uncheckedAppend(narrowPrecisionToFloat(0.7152 + 0.2848 * oneMinusAmount));
    matrix.uncheckedAppend(narrowPrecisionToFloat(0.0722 - 0.0722 * oneMinusAmount));
    endMatrixRow(matrix);

    matrix.uncheckedAppend(narrowPrecisionToFloat(0.2126 - 0.2126 * oneMinusAmount));
    matrix.uncheckedAppend(narrowPrecisionToFloat(0.7152 - 0.7152 * oneMinusAmount));
    matrix.uncheckedAppend(narrowPrecisionToFloat(0.0722 + 0.9278 * oneMinusAmount));
    endMatrixRow(matrix);

    lastMatrixRow(matrix);
    return matrix;
}

Vector<float> sepiaMatrix(double amount)
{
    double oneMinusAmount = clampTo(1 - amount, 0.0, 1.0);

    // See https://dvcs.w3.org/hg/FXTF/raw-file/tip/filters/index.html#sepiaEquivalent
    // for information on parameters.
    Vector<float> matrix;
    matrix.reserveInitialCapacity(20);

    matrix.uncheckedAppend(narrowPrecisionToFloat(0.393 + 0.607 * oneMinusAmount));
    matrix.uncheckedAppend(narrowPrecisionToFloat(0.769 - 0.769 * oneMinusAmount));
    matrix.uncheckedAppend(narrowPrecisionToFloat(0.189 - 0.189 * oneMinusAmount));
    endMatrixRow(matrix);

    matrix.uncheckedAppend(narrowPrecisionToFloat(0.349 - 0.349 * oneMinusAmount));
    matrix.uncheckedAppend(narrowPrecisionToFloat(0.686 + 0.314 * oneMinusAmount));
    matrix.uncheckedAppend(narrowPrecisionToFloat(0.168 - 0.168 * oneMinusAmount));
    endMatrixRow(matrix);

    matrix.uncheckedAppend(narrowPrecisionToFloat(0.272 - 0.272 * oneMinusAmount));
    matrix.uncheckedAppend(narrowPrecisionToFloat(0.534 - 0.534 * oneMinusAmount));
    matrix.uncheckedAppend(narrowPrecisionToFloat(0.131 + 0.869 * oneMinusAmount));
    endMatrixRow(matrix);

    lastMatrixRow(matrix);
    return matrix;
}

FloatRect computeReferenceBox(const Element& element, const FloatSize* zoomedReferenceBoxSize, float zoom)
{
    FloatSize size;
    if (zoomedReferenceBoxSize) {
        size = *zoomedReferenceBoxSize;
    } else if (element.inShadowIncludingDocument() && element.layoutObject() && element.layoutObject()->enclosingLayer()) {
        size = FloatSize(element.layoutObject()->enclosingLayer()->physicalBoundingBoxIncludingReflectionAndStackingChildren(LayoutPoint()).size());
    }
    size.scale(1.0f / zoom);
    return FloatRect(FloatPoint(), size);
}

} // namespace

FilterEffectBuilder::FilterEffectBuilder()
{
}

FilterEffectBuilder::~FilterEffectBuilder()
{
}

DEFINE_TRACE(FilterEffectBuilder)
{
    visitor->trace(m_lastEffect);
}

bool FilterEffectBuilder::build(Element* element, const FilterOperations& operations, float zoom, const FloatSize* zoomedReferenceBoxSize, const SkPaint* fillPaint, const SkPaint* strokePaint)
{
    // Create a parent filter for shorthand filters. These have already been scaled by the CSS code for page zoom, so scale is 1.0 here.
    Filter* parentFilter = Filter::create(1.0f);
    FilterEffect* previousEffect = parentFilter->getSourceGraphic();
    for (size_t i = 0; i < operations.operations().size(); ++i) {
        FilterEffect* effect = nullptr;
        FilterOperation* filterOperation = operations.operations().at(i).get();
        switch (filterOperation->type()) {
        case FilterOperation::REFERENCE: {
            Filter* referenceFilter = buildReferenceFilter(toReferenceFilterOperation(*filterOperation), zoomedReferenceBoxSize, fillPaint, strokePaint, *element, previousEffect, zoom);
            if (referenceFilter)
                effect = referenceFilter->lastEffect();
            break;
        }
        case FilterOperation::GRAYSCALE: {
            Vector<float> inputParameters = grayscaleMatrix(toBasicColorMatrixFilterOperation(filterOperation)->amount());
            effect = FEColorMatrix::create(parentFilter, FECOLORMATRIX_TYPE_MATRIX, inputParameters);
            break;
        }
        case FilterOperation::SEPIA: {
            Vector<float> inputParameters = sepiaMatrix(toBasicColorMatrixFilterOperation(filterOperation)->amount());
            effect = FEColorMatrix::create(parentFilter, FECOLORMATRIX_TYPE_MATRIX, inputParameters);
            break;
        }
        case FilterOperation::SATURATE: {
            Vector<float> inputParameters;
            inputParameters.append(narrowPrecisionToFloat(toBasicColorMatrixFilterOperation(filterOperation)->amount()));
            effect = FEColorMatrix::create(parentFilter, FECOLORMATRIX_TYPE_SATURATE, inputParameters);
            break;
        }
        case FilterOperation::HUE_ROTATE: {
            Vector<float> inputParameters;
            inputParameters.append(narrowPrecisionToFloat(toBasicColorMatrixFilterOperation(filterOperation)->amount()));
            effect = FEColorMatrix::create(parentFilter, FECOLORMATRIX_TYPE_HUEROTATE, inputParameters);
            break;
        }
        case FilterOperation::INVERT: {
            BasicComponentTransferFilterOperation* componentTransferOperation = toBasicComponentTransferFilterOperation(filterOperation);
            ComponentTransferFunction transferFunction;
            transferFunction.type = FECOMPONENTTRANSFER_TYPE_TABLE;
            Vector<float> transferParameters;
            transferParameters.append(narrowPrecisionToFloat(componentTransferOperation->amount()));
            transferParameters.append(narrowPrecisionToFloat(1 - componentTransferOperation->amount()));
            transferFunction.tableValues = transferParameters;

            ComponentTransferFunction nullFunction;
            effect = FEComponentTransfer::create(parentFilter, transferFunction, transferFunction, transferFunction, nullFunction);
            break;
        }
        case FilterOperation::OPACITY: {
            ComponentTransferFunction transferFunction;
            transferFunction.type = FECOMPONENTTRANSFER_TYPE_TABLE;
            Vector<float> transferParameters;
            transferParameters.append(0);
            transferParameters.append(narrowPrecisionToFloat(toBasicComponentTransferFilterOperation(filterOperation)->amount()));
            transferFunction.tableValues = transferParameters;

            ComponentTransferFunction nullFunction;
            effect = FEComponentTransfer::create(parentFilter, nullFunction, nullFunction, nullFunction, transferFunction);
            break;
        }
        case FilterOperation::BRIGHTNESS: {
            ComponentTransferFunction transferFunction;
            transferFunction.type = FECOMPONENTTRANSFER_TYPE_LINEAR;
            transferFunction.slope = narrowPrecisionToFloat(toBasicComponentTransferFilterOperation(filterOperation)->amount());
            transferFunction.intercept = 0;

            ComponentTransferFunction nullFunction;
            effect = FEComponentTransfer::create(parentFilter, transferFunction, transferFunction, transferFunction, nullFunction);
            break;
        }
        case FilterOperation::CONTRAST: {
            ComponentTransferFunction transferFunction;
            transferFunction.type = FECOMPONENTTRANSFER_TYPE_LINEAR;
            float amount = narrowPrecisionToFloat(toBasicComponentTransferFilterOperation(filterOperation)->amount());
            transferFunction.slope = amount;
            transferFunction.intercept = -0.5 * amount + 0.5;

            ComponentTransferFunction nullFunction;
            effect = FEComponentTransfer::create(parentFilter, transferFunction, transferFunction, transferFunction, nullFunction);
            break;
        }
        case FilterOperation::BLUR: {
            float stdDeviation = floatValueForLength(toBlurFilterOperation(filterOperation)->stdDeviation(), 0);
            effect = FEGaussianBlur::create(parentFilter, stdDeviation, stdDeviation);
            break;
        }
        case FilterOperation::DROP_SHADOW: {
            DropShadowFilterOperation* dropShadowOperation = toDropShadowFilterOperation(filterOperation);
            float stdDeviation = dropShadowOperation->stdDeviation();
            float x = dropShadowOperation->x();
            float y = dropShadowOperation->y();
            effect = FEDropShadow::create(parentFilter, stdDeviation, stdDeviation, x, y, dropShadowOperation->getColor(), 1);
            break;
        }
        case FilterOperation::BOX_REFLECT: {
            BoxReflectFilterOperation* boxReflectOperation = toBoxReflectFilterOperation(filterOperation);
            effect = FEBoxReflect::create(parentFilter, boxReflectOperation->reflection());
            break;
        }
        default:
            break;
        }

        if (effect) {
            if (filterOperation->type() != FilterOperation::REFERENCE) {
                // Unlike SVG, filters applied here should not clip to their primitive subregions.
                effect->setClipsToBounds(false);
                effect->setOperatingColorSpace(ColorSpaceDeviceRGB);
                effect->inputEffects().append(previousEffect);
            }
            if (previousEffect->originTainted())
                effect->setOriginTainted();
            previousEffect = effect;
        }
    }

    // We need to keep the old effects alive until this point, so that SVG reference filters
    // can share cached resources across frames.
    m_lastEffect = previousEffect;

    // If we didn't make any effects, tell our caller we are not valid
    if (!m_lastEffect.get())
        return false;

    return true;
}

Filter* FilterEffectBuilder::buildReferenceFilter(
    const ReferenceFilterOperation& referenceOperation,
    const FloatSize* zoomedReferenceBoxSize,
    const SkPaint* fillPaint,
    const SkPaint* strokePaint,
    Element& element,
    FilterEffect* previousEffect,
    float zoom)
{
    SVGFilterElement* filterElement = ReferenceFilterBuilder::resolveFilterReference(referenceOperation, element);
    if (!filterElement)
        return nullptr;

    const FloatRect referenceBox = computeReferenceBox(element, zoomedReferenceBoxSize, zoom);
    return buildReferenceFilter(*filterElement, referenceBox, fillPaint, strokePaint, previousEffect, zoom);
}

Filter* FilterEffectBuilder::buildReferenceFilter(
    SVGFilterElement& filterElement,
    const FloatRect& referenceBox,
    const SkPaint* fillPaint,
    const SkPaint* strokePaint,
    FilterEffect* previousEffect,
    float zoom,
    SVGFilterGraphNodeMap* nodeMap)
{
    FloatRect filterRegion = SVGLengthContext::resolveRectangle<SVGFilterElement>(&filterElement, filterElement.filterUnits()->currentValue()->enumValue(), referenceBox);
    // TODO(fs): We rely on the presence of a node map here to opt-in to the
    // check for an empty filter region. The reason for this is that we lack a
    // viewport to resolve against for HTML content. This is crbug.com/512453.
    if (nodeMap && filterRegion.isEmpty())
        return nullptr;

    bool primitiveBoundingBoxMode = filterElement.primitiveUnits()->currentValue()->enumValue() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;
    Filter::UnitScaling unitScaling = primitiveBoundingBoxMode ? Filter::BoundingBox : Filter::UserSpace;
    Filter* result = Filter::create(referenceBox, filterRegion, zoom, unitScaling);
    if (!previousEffect)
        previousEffect = result->getSourceGraphic();
    SVGFilterBuilder builder(previousEffect, nodeMap, fillPaint, strokePaint);
    builder.buildGraph(result, filterElement, referenceBox);
    result->setLastEffect(builder.lastEffect());
    return result;
}

} // namespace blink

