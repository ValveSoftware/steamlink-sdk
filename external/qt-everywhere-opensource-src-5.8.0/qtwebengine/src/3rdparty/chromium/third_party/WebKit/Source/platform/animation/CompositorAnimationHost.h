// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorAnimationHost_h
#define CompositorAnimationHost_h

#include "base/memory/ref_counted.h"
#include "cc/animation/animation_host.h"
#include "platform/PlatformExport.h"
#include "ui/gfx/geometry/vector2d.h"
#include "wtf/Noncopyable.h"

#include <memory>

namespace cc {
struct ScrollOffsetAnimationUpdate;
}

namespace blink {

// A compositor representation for cc::AnimationHost.
// This class wraps cc::AnimationHost and is currently only created from
// CompositorAnimationTimeline::compositorAnimationHost.
// TODO(ymalik): Correctly introduce CompositorAnimationHost to blink. See
// crbug.com/610763.
class PLATFORM_EXPORT CompositorAnimationHost {
public:
    explicit CompositorAnimationHost(cc::AnimationHost*);

    // TODO(ymalik): Remove when CompositorAnimationHost* optional nullable ptr
    // is returned. See crbug.com/610763.
    bool isNull() const;

    void adjustImplOnlyScrollOffsetAnimation(cc::ElementId, const gfx::Vector2dF& adjustment);
    void takeOverImplOnlyScrollOffsetAnimation(cc::ElementId);

private:
    cc::AnimationHost* m_animationHost;
};

} // namespace blink

#endif // CompositorAnimationTimeline_h
