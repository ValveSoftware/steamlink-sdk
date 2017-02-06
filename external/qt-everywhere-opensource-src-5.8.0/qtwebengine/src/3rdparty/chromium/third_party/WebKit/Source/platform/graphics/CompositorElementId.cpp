// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CompositorElementId.h"

namespace blink {

CompositorElementId createCompositorElementId(int domNodeId, CompositorSubElementId subElementId)
{
    return CompositorElementId(domNodeId, static_cast<int>(subElementId));
}

} // namespace blink
