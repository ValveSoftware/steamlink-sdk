// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/proxy_common.h"

#include "cc/proto/begin_main_frame_and_commit_state.pb.h"
#include "cc/test/begin_frame_args_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(ProxyCommonUnittest, SerializeBeginMainFrameAndCommitState) {
  BeginMainFrameAndCommitState begin_main_frame_state;
  begin_main_frame_state.begin_frame_id = 5;

  begin_main_frame_state.begin_frame_args = BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, base::TimeTicks::FromInternalValue(4),
      base::TimeTicks::FromInternalValue(7),
      base::TimeDelta::FromInternalValue(9), BeginFrameArgs::NORMAL);
  begin_main_frame_state.begin_frame_args.on_critical_path = false;

  LayerTreeHostCommon::ScrollUpdateInfo scroll1;
  scroll1.layer_id = 1;
  scroll1.scroll_delta = gfx::Vector2d(5, 10);
  LayerTreeHostCommon::ScrollUpdateInfo scroll2;
  scroll2.layer_id = 2;
  scroll2.scroll_delta = gfx::Vector2d(1, 5);
  begin_main_frame_state.scroll_info.reset(new ScrollAndScaleSet());
  begin_main_frame_state.scroll_info->scrolls.push_back(scroll1);
  begin_main_frame_state.scroll_info->scrolls.push_back(scroll2);

  begin_main_frame_state.scroll_info->page_scale_delta = 0.3f;
  begin_main_frame_state.scroll_info->elastic_overscroll_delta =
      gfx::Vector2dF(0.5f, 0.6f);
  begin_main_frame_state.scroll_info->top_controls_delta = 0.9f;

  begin_main_frame_state.memory_allocation_limit_bytes = 16;
  begin_main_frame_state.evicted_ui_resources = false;

  proto::BeginMainFrameAndCommitState proto;
  begin_main_frame_state.ToProtobuf(&proto);

  BeginMainFrameAndCommitState new_begin_main_frame_state;
  new_begin_main_frame_state.FromProtobuf(proto);

  EXPECT_EQ(new_begin_main_frame_state.begin_frame_id,
            begin_main_frame_state.begin_frame_id);
  EXPECT_EQ(new_begin_main_frame_state.begin_frame_args,
            begin_main_frame_state.begin_frame_args);
  EXPECT_TRUE(begin_main_frame_state.scroll_info->EqualsForTesting(
      *new_begin_main_frame_state.scroll_info.get()));
  EXPECT_EQ(new_begin_main_frame_state.memory_allocation_limit_bytes,
            begin_main_frame_state.memory_allocation_limit_bytes);
  EXPECT_EQ(new_begin_main_frame_state.evicted_ui_resources,
            begin_main_frame_state.evicted_ui_resources);
}

}  // namespace
}  // namespace cc
