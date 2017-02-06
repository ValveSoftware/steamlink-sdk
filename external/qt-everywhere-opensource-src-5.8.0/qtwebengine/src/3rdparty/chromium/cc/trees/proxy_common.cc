// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/proxy_common.h"

#include "cc/proto/begin_main_frame_and_commit_state.pb.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

BeginMainFrameAndCommitState::BeginMainFrameAndCommitState() {}

BeginMainFrameAndCommitState::~BeginMainFrameAndCommitState() {}

void BeginMainFrameAndCommitState::ToProtobuf(
    proto::BeginMainFrameAndCommitState* proto) const {
  proto->set_begin_frame_id(begin_frame_id);
  begin_frame_args.ToProtobuf(proto->mutable_begin_frame_args());
  scroll_info->ToProtobuf(proto->mutable_scroll_info());
  proto->set_memory_allocation_limit_bytes(memory_allocation_limit_bytes);
  proto->set_evicted_ui_resources(evicted_ui_resources);
}

void BeginMainFrameAndCommitState::FromProtobuf(
    const proto::BeginMainFrameAndCommitState& proto) {
  begin_frame_id = proto.begin_frame_id();
  begin_frame_args.FromProtobuf(proto.begin_frame_args());
  scroll_info.reset(new ScrollAndScaleSet());
  scroll_info->FromProtobuf(proto.scroll_info());
  memory_allocation_limit_bytes = proto.memory_allocation_limit_bytes();
  evicted_ui_resources = proto.evicted_ui_resources();
}

}  // namespace cc
