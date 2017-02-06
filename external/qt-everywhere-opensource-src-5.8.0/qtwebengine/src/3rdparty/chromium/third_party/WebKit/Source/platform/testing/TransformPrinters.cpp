// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/TransformPrinters.h"

#include "platform/transforms/AffineTransform.h"
#include "platform/transforms/TransformationMatrix.h"
#include <ostream> // NOLINT

namespace blink {

void PrintTo(const AffineTransform& transform, std::ostream* os)
{
    AffineTransform::DecomposedType decomposition;
    if (!transform.decompose(decomposition)) {
        *os << "AffineTransform(degenerate)";
        return;
    }

    if (transform.isIdentityOrTranslation()) {
        *os << "AffineTransform(translation=(" << decomposition.translateX << "," << decomposition.translateY << "))";
        return;
    }

    *os << "AffineTransform("
        << "translation=(" << decomposition.translateX << "," << decomposition.translateY << ")"
        << ", scale=(" << decomposition.scaleX << "," << decomposition.scaleY << ")"
        << ", angle=(" << decomposition.angle << ")"
        << ", remainder=(" << decomposition.remainderA << "," << decomposition.remainderB << "," << decomposition.remainderC << "," << decomposition.remainderD << ")"
        << ", translate=(" << decomposition.translateX << "," << decomposition.translateY << ")"
        << ")";
}

void PrintTo(const TransformationMatrix& matrix, std::ostream* os)
{
    TransformationMatrix::DecomposedType decomposition;
    if (!matrix.decompose(decomposition)) {
        *os << "TransformationMatrix(degenerate)";
        return;
    }

    if (matrix.isIdentityOrTranslation()) {
        *os << "TransformationMatrix(translation=(" << decomposition.translateX << "," << decomposition.translateY << "," << decomposition.translateZ << "))";
        return;
    }

    *os << "TransformationMatrix("
        << "translation=(" << decomposition.translateX << "," << decomposition.translateY << "," << decomposition.translateZ << ")"
        << ", scale=(" << decomposition.scaleX << "," << decomposition.scaleY << "," << decomposition.scaleZ << ")"
        << ", skew=(" << decomposition.skewXY << "," << decomposition.skewXZ << "," << decomposition.skewYZ << ")"
        << ", quaternion=(" << decomposition.quaternionX << "," << decomposition.quaternionY << "," << decomposition.quaternionZ << "," << decomposition.quaternionW << ")"
        << ", perspective=(" << decomposition.perspectiveX << "," << decomposition.perspectiveY << "," << decomposition.perspectiveZ << "," << decomposition.perspectiveW << ")"
        << ")";
}

} // namespace blink
