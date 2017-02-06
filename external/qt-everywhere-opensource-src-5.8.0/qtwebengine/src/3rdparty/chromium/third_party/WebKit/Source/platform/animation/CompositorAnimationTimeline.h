// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorAnimationTimeline_h
#define CompositorAnimationTimeline_h

#include "base/memory/ref_counted.h"
#include "cc/animation/animation_timeline.h"
#include "platform/PlatformExport.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class CompositorAnimationHost;
class CompositorAnimationPlayerClient;

// A compositor representation for cc::AnimationTimeline.
class PLATFORM_EXPORT CompositorAnimationTimeline {
    WTF_MAKE_NONCOPYABLE(CompositorAnimationTimeline);
public:
    static std::unique_ptr<CompositorAnimationTimeline> create()
    {
        return wrapUnique(new CompositorAnimationTimeline());
    }

    ~CompositorAnimationTimeline();

    cc::AnimationTimeline* animationTimeline() const;
    // TODO(ymalik): Currently we just wrap cc::AnimationHost in
    // CompositorAnimationHost. Correctly introduce CompositorAnimationHost
    // to blink. See crbug.com/610763.
    CompositorAnimationHost compositorAnimationHost();

    void playerAttached(const CompositorAnimationPlayerClient&);
    void playerDestroyed(const CompositorAnimationPlayerClient&);

private:
    CompositorAnimationTimeline();

    scoped_refptr<cc::AnimationTimeline> m_animationTimeline;
};

} // namespace blink

#endif // CompositorAnimationTimeline_h
