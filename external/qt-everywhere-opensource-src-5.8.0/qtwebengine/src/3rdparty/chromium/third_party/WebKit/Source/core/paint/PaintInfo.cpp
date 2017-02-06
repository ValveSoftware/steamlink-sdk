// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintInfo.h"

namespace blink {

void PaintInfo::updateCullRect(const AffineTransform& localToParentTransform)
{
    m_cullRect.updateCullRect(localToParentTransform);
}

} // namespace blink
