// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorElementId_h
#define CompositorElementId_h

#include "cc/animation/element_id.h"
#include "platform/PlatformExport.h"

namespace blink {

enum class CompositorSubElementId {
    Primary,
    Scroll,
    LinkHighlight
};

using CompositorElementId = cc::ElementId;

CompositorElementId PLATFORM_EXPORT createCompositorElementId(int domNodeId, CompositorSubElementId);

} // namespace blink

#endif // CompositorElementId_h
