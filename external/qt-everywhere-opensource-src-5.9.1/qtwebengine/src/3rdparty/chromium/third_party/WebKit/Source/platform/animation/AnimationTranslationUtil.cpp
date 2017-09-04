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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/animation/AnimationTranslationUtil.h"

#include "platform/animation/CompositorTransformOperations.h"
#include "platform/transforms/InterpolatedTransformOperation.h"
#include "platform/transforms/Matrix3DTransformOperation.h"
#include "platform/transforms/MatrixTransformOperation.h"
#include "platform/transforms/PerspectiveTransformOperation.h"
#include "platform/transforms/RotateTransformOperation.h"
#include "platform/transforms/ScaleTransformOperation.h"
#include "platform/transforms/SkewTransformOperation.h"
#include "platform/transforms/TransformOperations.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/transforms/TranslateTransformOperation.h"

namespace blink {

void toCompositorTransformOperations(
    const TransformOperations& transformOperations,
    CompositorTransformOperations* outTransformOperations) {
  // We need to do a deep copy the transformOperations may contain ref pointers
  // to TransformOperation objects.
  for (const auto& operation : transformOperations.operations()) {
    switch (operation->type()) {
      case TransformOperation::ScaleX:
      case TransformOperation::ScaleY:
      case TransformOperation::ScaleZ:
      case TransformOperation::Scale3D:
      case TransformOperation::Scale: {
        auto transform =
            static_cast<const ScaleTransformOperation*>(operation.get());
        outTransformOperations->appendScale(transform->x(), transform->y(),
                                            transform->z());
        break;
      }
      case TransformOperation::TranslateX:
      case TransformOperation::TranslateY:
      case TransformOperation::TranslateZ:
      case TransformOperation::Translate3D:
      case TransformOperation::Translate: {
        auto transform =
            static_cast<const TranslateTransformOperation*>(operation.get());
        DCHECK(transform->x().isFixed() && transform->y().isFixed());
        outTransformOperations->appendTranslate(
            transform->x().value(), transform->y().value(), transform->z());
        break;
      }
      case TransformOperation::RotateX:
      case TransformOperation::RotateY:
      case TransformOperation::Rotate3D:
      case TransformOperation::Rotate: {
        auto transform =
            static_cast<const RotateTransformOperation*>(operation.get());
        outTransformOperations->appendRotate(
            transform->x(), transform->y(), transform->z(), transform->angle());
        break;
      }
      case TransformOperation::SkewX:
      case TransformOperation::SkewY:
      case TransformOperation::Skew: {
        auto transform =
            static_cast<const SkewTransformOperation*>(operation.get());
        outTransformOperations->appendSkew(transform->angleX(),
                                           transform->angleY());
        break;
      }
      case TransformOperation::Matrix: {
        auto transform =
            static_cast<const MatrixTransformOperation*>(operation.get());
        TransformationMatrix m = transform->matrix();
        outTransformOperations->appendMatrix(
            TransformationMatrix::toSkMatrix44(m));
        break;
      }
      case TransformOperation::Matrix3D: {
        auto transform =
            static_cast<const Matrix3DTransformOperation*>(operation.get());
        TransformationMatrix m = transform->matrix();
        outTransformOperations->appendMatrix(
            TransformationMatrix::toSkMatrix44(m));
        break;
      }
      case TransformOperation::Perspective: {
        auto transform =
            static_cast<const PerspectiveTransformOperation*>(operation.get());
        outTransformOperations->appendPerspective(transform->perspective());
        break;
      }
      case TransformOperation::Interpolated: {
        TransformationMatrix m;
        operation->apply(m, FloatSize());
        outTransformOperations->appendMatrix(
            TransformationMatrix::toSkMatrix44(m));
        break;
      }
      case TransformOperation::Identity:
        outTransformOperations->appendIdentity();
        break;
      case TransformOperation::None:
        // Do nothing.
        break;
    }  // switch
  }    // for each operation
}

}  // namespace blink
