// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_writing_mode.h"

#include "platform/text/WritingMode.h"
#include "wtf/Assertions.h"

namespace blink {

NGWritingMode FromPlatformWritingMode(WritingMode mode) {
  switch (mode) {
    case TopToBottomWritingMode:
      return HorizontalTopBottom;
    case RightToLeftWritingMode:
      return VerticalRightLeft;
    case LeftToRightWritingMode:
      return VerticalLeftRight;
    default:
      NOTREACHED();
      return HorizontalTopBottom;
  }
}

}  // namespace blink
