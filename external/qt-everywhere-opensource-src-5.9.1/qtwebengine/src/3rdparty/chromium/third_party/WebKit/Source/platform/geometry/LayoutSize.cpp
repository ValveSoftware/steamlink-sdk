// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/LayoutSize.h"

#include "wtf/text/WTFString.h"

namespace blink {

String LayoutSize::toString() const {
  return String::format("%sx%s", width().toString().ascii().data(),
                        height().toString().ascii().data());
}

}  // namespace blink
