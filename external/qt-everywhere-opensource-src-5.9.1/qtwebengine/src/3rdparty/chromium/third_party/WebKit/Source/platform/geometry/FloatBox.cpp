// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/FloatBox.h"

#include "wtf/text/WTFString.h"

namespace blink {

String FloatBox::toString() const {
  return String::format("%lg,%lg,%lg %lgx%lgx%lg", x(), y(), z(), width(),
                        height(), depth());
}

}  // namespace blink
