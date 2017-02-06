/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/animation/AnimationTranslationUtil.h"

#include "platform/animation/CompositorTransformOperations.h"
#include "platform/graphics/CompositorFilterOperations.h"
#include "platform/graphics/filters/FilterOperations.h"
#include "platform/transforms/Matrix3DTransformOperation.h"
#include "platform/transforms/RotateTransformOperation.h"
#include "platform/transforms/ScaleTransformOperation.h"
#include "platform/transforms/TransformOperations.h"
#include "platform/transforms/TranslateTransformOperation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

TEST(AnimationTranslationUtilTest, transformsWork)
{
    TransformOperations ops;
    std::unique_ptr<CompositorTransformOperations> outOps = CompositorTransformOperations::create();

    ops.operations().append(TranslateTransformOperation::create(Length(2, Fixed), Length(0, Fixed), TransformOperation::TranslateX));
    ops.operations().append(RotateTransformOperation::create(0.1, 0.2, 0.3, 200000.4, TransformOperation::Rotate3D));
    ops.operations().append(ScaleTransformOperation::create(50.2, 100, -4, TransformOperation::Scale3D));
    toCompositorTransformOperations(ops, outOps.get());

    EXPECT_EQ(3UL, outOps->asTransformOperations().size());
    const float err = 0.0001;

    auto& op0 = outOps->asTransformOperations().at(0);
    EXPECT_EQ(cc::TransformOperation::TRANSFORM_OPERATION_TRANSLATE, op0.type);
    EXPECT_NEAR(op0.translate.x, 2.0f, err);
    EXPECT_NEAR(op0.translate.y, 0.0f, err);
    EXPECT_NEAR(op0.translate.z, 0.0f, err);

    auto& op1 = outOps->asTransformOperations().at(1);
    EXPECT_EQ(cc::TransformOperation::TRANSFORM_OPERATION_ROTATE, op1.type);
    EXPECT_NEAR(op1.rotate.axis.x, 0.1f, err);
    EXPECT_NEAR(op1.rotate.axis.y, 0.2f, err);
    EXPECT_NEAR(op1.rotate.axis.z, 0.3f, err);
    EXPECT_NEAR(op1.rotate.angle, 200000.4f, 0.01f);

    auto& op2 = outOps->asTransformOperations().at(2);
    EXPECT_EQ(cc::TransformOperation::TRANSFORM_OPERATION_SCALE, op2.type);
    EXPECT_NEAR(op2.scale.x, 50.2f, err);
    EXPECT_NEAR(op2.scale.y, 100.0f, err);
    EXPECT_NEAR(op2.scale.z, -4.0f, err);
}

TEST(AnimationTranslationUtilTest, filtersWork)
{
    FilterOperations ops;
    std::unique_ptr<CompositorFilterOperations> outOps = CompositorFilterOperations::create();

    ops.operations().append(BasicColorMatrixFilterOperation::create(0.5, FilterOperation::SATURATE));
    ops.operations().append(BasicColorMatrixFilterOperation::create(0.2, FilterOperation::GRAYSCALE));
    ops.operations().append(BasicColorMatrixFilterOperation::create(0.8, FilterOperation::SEPIA));
    ops.operations().append(BasicComponentTransferFilterOperation::create(0.1, FilterOperation::OPACITY));
    toCompositorFilterOperations(ops, outOps.get());

    EXPECT_EQ(4UL, outOps->asFilterOperations().size());
    const float err = 0.0001;

    auto& op0 = outOps->asFilterOperations().at(0);
    EXPECT_EQ(cc::FilterOperation::SATURATE, op0.type());
    EXPECT_NEAR(op0.amount(), 0.5f, err);

    auto& op1 = outOps->asFilterOperations().at(1);
    EXPECT_EQ(cc::FilterOperation::GRAYSCALE, op1.type());
    EXPECT_NEAR(op1.amount(), 0.2f, err);

    auto& op2 = outOps->asFilterOperations().at(2);
    EXPECT_EQ(cc::FilterOperation::SEPIA, op2.type());
    EXPECT_NEAR(op2.amount(), 0.8f, err);

    auto& op3 = outOps->asFilterOperations().at(3);
    EXPECT_EQ(cc::FilterOperation::OPACITY, op3.type());
    EXPECT_NEAR(op3.amount(), 0.1f, err);
}

} // namespace blink
