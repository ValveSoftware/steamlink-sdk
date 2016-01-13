// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/video_layer_impl.h"

#include "cc/layers/video_frame_provider_client_impl.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "cc/test/fake_video_frame_provider.h"
#include "cc/test/layer_test_common.h"
#include "cc/trees/single_thread_proxy.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(VideoLayerImplTest, Occlusion) {
  gfx::Size layer_size(1000, 1000);
  gfx::Size viewport_size(1000, 1000);

  LayerTestCommon::LayerImplTest impl;
  DebugScopedSetImplThreadAndMainThreadBlocked thread(impl.proxy());

  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateFrame(media::VideoFrame::YV12,
                                     gfx::Size(10, 10),
                                     gfx::Rect(10, 10),
                                     gfx::Size(10, 10),
                                     base::TimeDelta());
  FakeVideoFrameProvider provider;
  provider.set_frame(video_frame);

  VideoLayerImpl* video_layer_impl =
      impl.AddChildToRoot<VideoLayerImpl>(&provider);
  video_layer_impl->SetBounds(layer_size);
  video_layer_impl->SetContentBounds(layer_size);
  video_layer_impl->SetDrawsContent(true);

  impl.CalcDrawProps(viewport_size);

  {
    SCOPED_TRACE("No occlusion");
    gfx::Rect occluded;
    impl.AppendQuadsWithOcclusion(video_layer_impl, occluded);

    LayerTestCommon::VerifyQuadsExactlyCoverRect(impl.quad_list(),
                                                 gfx::Rect(layer_size));
    EXPECT_EQ(1u, impl.quad_list().size());
  }

  {
    SCOPED_TRACE("Full occlusion");
    gfx::Rect occluded(video_layer_impl->visible_content_rect());
    impl.AppendQuadsWithOcclusion(video_layer_impl, occluded);

    LayerTestCommon::VerifyQuadsExactlyCoverRect(impl.quad_list(), gfx::Rect());
    EXPECT_EQ(impl.quad_list().size(), 0u);
  }

  {
    SCOPED_TRACE("Partial occlusion");
    gfx::Rect occluded(200, 0, 800, 1000);
    impl.AppendQuadsWithOcclusion(video_layer_impl, occluded);

    size_t partially_occluded_count = 0;
    LayerTestCommon::VerifyQuadsCoverRectWithOcclusion(
        impl.quad_list(),
        gfx::Rect(layer_size),
        occluded,
        &partially_occluded_count);
    // The layer outputs one quad, which is partially occluded.
    EXPECT_EQ(1u, impl.quad_list().size());
    EXPECT_EQ(1u, partially_occluded_count);
  }
}

TEST(VideoLayerImplTest, DidBecomeActiveShouldSetActiveVideoLayer) {
  LayerTestCommon::LayerImplTest impl;
  DebugScopedSetImplThreadAndMainThreadBlocked thread(impl.proxy());

  FakeVideoFrameProvider provider;
  VideoLayerImpl* video_layer_impl =
      impl.AddChildToRoot<VideoLayerImpl>(&provider);

  VideoFrameProviderClientImpl* client =
      static_cast<VideoFrameProviderClientImpl*>(provider.client());
  ASSERT_TRUE(client);
  EXPECT_FALSE(client->active_video_layer());

  video_layer_impl->DidBecomeActive();
  EXPECT_EQ(video_layer_impl, client->active_video_layer());
}

}  // namespace
}  // namespace cc
