// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimationHost.h"

#include "base/memory/ref_counted.h"
#include "platform/animation/CompositorAnimationTimeline.h"
#include "platform/testing/CompositorTest.h"
#include "platform/testing/WebLayerTreeViewImplForTesting.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class CompositorAnimationHostTest : public CompositorTest {
};

TEST_F(CompositorAnimationHostTest, AnimationHostNullWhenTimelineDetached)
{
    std::unique_ptr<CompositorAnimationTimeline> timeline = CompositorAnimationTimeline::create();

    scoped_refptr<cc::AnimationTimeline> ccTimeline = timeline->animationTimeline();
    EXPECT_FALSE(ccTimeline->animation_host());
    EXPECT_TRUE(timeline->compositorAnimationHost().isNull());

    std::unique_ptr<WebLayerTreeView> layerTreeHost = wrapUnique(new WebLayerTreeViewImplForTesting);
    DCHECK(layerTreeHost);

    layerTreeHost->attachCompositorAnimationTimeline(timeline->animationTimeline());
    EXPECT_FALSE(timeline->compositorAnimationHost().isNull());

    layerTreeHost->detachCompositorAnimationTimeline(timeline->animationTimeline());
    EXPECT_TRUE(timeline->compositorAnimationHost().isNull());
}

} // namespace blink
