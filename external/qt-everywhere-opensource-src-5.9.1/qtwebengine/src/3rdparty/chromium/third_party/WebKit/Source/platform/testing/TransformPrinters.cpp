// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/TransformPrinters.h"

#include "platform/transforms/AffineTransform.h"
#include "platform/transforms/TransformationMatrix.h"
#include "wtf/text/WTFString.h"
#include <ostream>  // NOLINT

namespace blink {

void PrintTo(const AffineTransform& transform, std::ostream* os) {
  *os << transform.toString();
}

void PrintTo(const TransformationMatrix& matrix, std::ostream* os) {
  *os << matrix.toString();
}

}  // namespace blink
