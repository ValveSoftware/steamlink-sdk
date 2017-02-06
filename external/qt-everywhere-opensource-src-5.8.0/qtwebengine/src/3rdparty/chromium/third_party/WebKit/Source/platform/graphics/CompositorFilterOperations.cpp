// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CompositorFilterOperations.h"

#include "third_party/skia/include/core/SkImageFilter.h"

namespace blink {

CompositorFilterOperations::CompositorFilterOperations()
{
}

const cc::FilterOperations& CompositorFilterOperations::asFilterOperations() const
{
    return m_filterOperations;
}

void CompositorFilterOperations::appendGrayscaleFilter(float amount)
{
    m_filterOperations.Append(cc::FilterOperation::CreateGrayscaleFilter(amount));
}

void CompositorFilterOperations::appendSepiaFilter(float amount)
{
    m_filterOperations.Append(cc::FilterOperation::CreateSepiaFilter(amount));
}

void CompositorFilterOperations::appendSaturateFilter(float amount)
{
    m_filterOperations.Append(cc::FilterOperation::CreateSaturateFilter(amount));
}

void CompositorFilterOperations::appendHueRotateFilter(float amount)
{
    m_filterOperations.Append(cc::FilterOperation::CreateHueRotateFilter(amount));
}

void CompositorFilterOperations::appendInvertFilter(float amount)
{
    m_filterOperations.Append(cc::FilterOperation::CreateInvertFilter(amount));
}

void CompositorFilterOperations::appendBrightnessFilter(float amount)
{
    m_filterOperations.Append(
        cc::FilterOperation::CreateBrightnessFilter(amount));
}

void CompositorFilterOperations::appendContrastFilter(float amount)
{
    m_filterOperations.Append(cc::FilterOperation::CreateContrastFilter(amount));
}

void CompositorFilterOperations::appendOpacityFilter(float amount)
{
    m_filterOperations.Append(cc::FilterOperation::CreateOpacityFilter(amount));
}

void CompositorFilterOperations::appendBlurFilter(float amount)
{
    m_filterOperations.Append(cc::FilterOperation::CreateBlurFilter(amount));
}

void CompositorFilterOperations::appendDropShadowFilter(IntPoint offset, float stdDeviation, Color color)
{
    gfx::Point gfxOffset(offset.x(), offset.y());
    m_filterOperations.Append(cc::FilterOperation::CreateDropShadowFilter(gfxOffset, stdDeviation, color.rgb()));
}

void CompositorFilterOperations::appendColorMatrixFilter(SkScalar matrix[20])
{
    m_filterOperations.Append(
        cc::FilterOperation::CreateColorMatrixFilter(matrix));
}

void CompositorFilterOperations::appendZoomFilter(float amount, int inset)
{
    m_filterOperations.Append(
        cc::FilterOperation::CreateZoomFilter(amount, inset));
}

void CompositorFilterOperations::appendSaturatingBrightnessFilter(float amount)
{
    m_filterOperations.Append(
        cc::FilterOperation::CreateSaturatingBrightnessFilter(amount));
}

void CompositorFilterOperations::appendReferenceFilter(sk_sp<SkImageFilter> imageFilter)
{
    m_filterOperations.Append(
        cc::FilterOperation::CreateReferenceFilter(std::move(imageFilter)));
}

void CompositorFilterOperations::clear()
{
    m_filterOperations.Clear();
}

bool CompositorFilterOperations::isEmpty() const
{
    return m_filterOperations.IsEmpty();
}

} // namespace blink
