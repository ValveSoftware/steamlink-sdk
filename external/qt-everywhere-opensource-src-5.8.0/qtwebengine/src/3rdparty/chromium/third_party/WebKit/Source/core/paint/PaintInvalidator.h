// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintInvalidator_h
#define PaintInvalidator_h

#include "core/layout/PaintInvalidationState.h"
#include "wtf/Optional.h"
#include "wtf/Vector.h"

namespace blink {

class FrameView;
class LayoutObject;
struct PaintPropertyTreeBuilderContext;

// TODO(wangxianzhu): Move applicable PaintInvalidationState code into PaintInvalidator.
using PaintInvalidatorContext = PaintInvalidationState;

class PaintInvalidator {
public:
    // TODO(wangxianzhu): Avoid Optional<> by using copy-and-update pattern for PaintInvalidatorContext.
    void invalidatePaintIfNeeded(FrameView&, const PaintPropertyTreeBuilderContext&, Optional<PaintInvalidatorContext>&);
    void invalidatePaintIfNeeded(const LayoutObject&, const PaintPropertyTreeBuilderContext&, const PaintInvalidatorContext&, Optional<PaintInvalidatorContext>&);

    // Process objects needing paint invalidation on the next frame.
    // See the definition of PaintInvalidationDelayedFull for more details.
    void processPendingDelayedPaintInvalidations();

private:
    Vector<const LayoutObject*> m_pendingDelayedPaintInvalidations;
};

} // namespace blink

#endif // PaintInvalidator_h
