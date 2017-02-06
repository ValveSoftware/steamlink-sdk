// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimationTimeline.h"

#include "base/memory/ref_counted.h"
#include "cc/animation/animation_host.h"
#include "platform/animation/CompositorAnimationPlayer.h"
#include "platform/testing/CompositorTest.h"
#include "platform/testing/WebLayerTreeViewImplForTesting.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class CompositorAnimationTimelineTest : public CompositorTest {
};

TEST_F(CompositorAnimationTimelineTest, CompositorTimelineDeletionDetachesFromAnimationHost)
{
    std::unique_ptr<CompositorAnimationTimeline> timeline = CompositorAnimationTimeline::create();

    scoped_refptr<cc::AnimationTimeline> ccTimeline = timeline->animationTimeline();
    EXPECT_FALSE(ccTimeline->animation_host());

    std::unique_ptr<WebLayerTreeView> layerTreeHost = wrapUnique(new WebLayerTreeViewImplForTesting);
    DCHECK(layerTreeHost);

    layerTreeHost->attachCompositorAnimationTimeline(timeline->animationTimeline());
    cc::AnimationHost* animationHost = ccTimeline->animation_host();
    EXPECT_TRUE(animationHost);
    EXPECT_TRUE(animationHost->GetTimelineById(ccTimeline->id()));

    // Delete CompositorAnimationTimeline while attached to host.
    timeline = nullptr;

    EXPECT_FALSE(ccTimeline->animation_host());
    EXPECT_FALSE(animationHost->GetTimelineById(ccTimeline->id()));
}

} // namespace blink
