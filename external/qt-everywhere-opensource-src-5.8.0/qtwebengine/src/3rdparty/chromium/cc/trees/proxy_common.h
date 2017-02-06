// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROXY_COMMON_H_
#define CC_TREES_PROXY_COMMON_H_

#include <stddef.h>

#include "base/callback_forward.h"
#include "cc/base/cc_export.h"
#include "cc/output/begin_frame_args.h"
#include "cc/trees/layer_tree_host_common.h"

namespace cc {

namespace proto {
class BeginMainFrameAndCommitState;
}

class LayerTreeHost;

using BeginFrameCallbackList = std::vector<base::Closure>;

struct CC_EXPORT BeginMainFrameAndCommitState {
  BeginMainFrameAndCommitState();
  ~BeginMainFrameAndCommitState();

  unsigned int begin_frame_id = 0;
  BeginFrameArgs begin_frame_args;
  std::unique_ptr<BeginFrameCallbackList> begin_frame_callbacks;
  std::unique_ptr<ScrollAndScaleSet> scroll_info;
  size_t memory_allocation_limit_bytes = 0;
  bool evicted_ui_resources = false;

  void ToProtobuf(proto::BeginMainFrameAndCommitState* proto) const;
  void FromProtobuf(const proto::BeginMainFrameAndCommitState& proto);
};

}  // namespace cc

#endif  // CC_TREES_PROXY_COMMON_H_
